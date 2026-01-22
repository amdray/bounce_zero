// level.c - Парсер уровней + отрисовка с атласом PNG (с поддержкой трансформаций)
#include "level.h"

// Цвета фона тайлов определены в level.h (из Java BounceConst)
#include "tile_table.h"
#include "graphics.h"
#include "game.h"  // Для анимации двери
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <pspkernel.h>  // Для sceKernelDcacheWritebackRange

// Статические переменные для респауна (как в оригинальном Java коде)
static int s_respawn_x = 0, s_respawn_y = 0;

// Формат пути к файлам уровней
#define LEVEL_PATH_FORMAT "levels/J2MElvl.%03d"

// Параметры полосок EXIT тайла (двери)
#define EXIT_STRIPE_1_X      0    // Первая полоска (фон)
#define EXIT_STRIPE_1_WIDTH  24   // Ширина первой полоски (вся область)
#define EXIT_STRIPE_2_X      4    // Голубая полоска
#define EXIT_STRIPE_2_WIDTH  16   // Ширина голубой полоски
#define EXIT_STRIPE_3_X      6    // Красная полоска
#define EXIT_STRIPE_3_WIDTH  10   // Ширина красной полоски
#define EXIT_STRIPE_4_X      10   // Четвертая полоска
#define EXIT_STRIPE_4_WIDTH  4    // Ширина четвертой полоски

// --- Текстуры/атлас (инкапсулированы через level_get_*) ---
static texture_t* s_tileset = NULL;
static int s_tiles_per_row = 0;


// --- Dynamic sprite validation ---
static inline int get_max_sprite_index(void) {
    if (!s_tileset || s_tiles_per_row <= 0) return -1;
    // actual_height используется вместо height, так как может быть больше из-за POT требований PSP
    int tiles_per_col = s_tileset->actual_height / TILE_SIZE;
    return s_tiles_per_row * tiles_per_col;
}

static inline bool is_sprite_valid(int sprite_idx) {
    int max_idx = get_max_sprite_index();
    return max_idx >= 0 && sprite_idx >= 0 && sprite_idx < max_idx;
}

// --- Ring foreground queue (draw after the ball) ---
typedef struct { int sprite_idx; float x, y; int transform; } hoop_fg_item_t;
static hoop_fg_item_t s_hoop_fg[RING_FG_QUEUE_MAX];
static int s_hoop_fg_count = 0;
static inline void hoop_fg_clear(void){ s_hoop_fg_count = 0; }

static inline void hoop_fg_push(int sprite_idx, float x, float y, int transform){
    if (s_hoop_fg_count < (int)(sizeof(s_hoop_fg)/sizeof(s_hoop_fg[0]))){
        s_hoop_fg[s_hoop_fg_count++] = (hoop_fg_item_t){ sprite_idx, x, y, transform };
    }
}

// Map TileTransform -> png_transform_t
static inline png_transform_t map_tf_to_png(int tf){
    switch (tf){
        case TF_FLIP_X:  return PNG_TRANSFORM_FLIP_X;
        case TF_FLIP_Y:  return PNG_TRANSFORM_FLIP_Y;
        case TF_FLIP_XY: return PNG_TRANSFORM_ROT_180; // FLIP_X+FLIP_Y эквивалент 180°
        case TF_ROT_90:  return PNG_TRANSFORM_ROT_90;
        case TF_ROT_180: return PNG_TRANSFORM_ROT_180;
        case TF_ROT_270: return PNG_TRANSFORM_ROT_270;
        case TF_ROT_270_FLIP_X: return PNG_TRANSFORM_ROT_270_FLIP_X;
        case TF_ROT_270_FLIP_Y: return PNG_TRANSFORM_ROT_270_FLIP_Y;

        case TF_ROT_270_FLIP_XY: return PNG_TRANSFORM_ROT_270_FLIP_XY;
        default:         return PNG_TRANSFORM_IDENTITY;
    }
}

static void hoop_fg_flush(void){
    if (!s_tileset || s_tiles_per_row <= 0) { s_hoop_fg_count = 0; return; }
    for (int i = 0; i < s_hoop_fg_count; ++i){
        int idx = s_hoop_fg[i].sprite_idx;
        if (!is_sprite_valid(idx)) continue;
        int col = idx % s_tiles_per_row;
        int row = idx / s_tiles_per_row;
        int srcX = col * TILE_SIZE;
        int srcY = row * TILE_SIZE;
        sprite_rect_t r = png_create_sprite_rect(s_tileset, srcX, srcY, TILE_SIZE, TILE_SIZE);
        png_transform_t xf = map_tf_to_png(s_hoop_fg[i].transform); // alt_transform now used from queue
        if (xf == PNG_TRANSFORM_IDENTITY){
            png_draw_sprite(s_tileset, &r, (int)s_hoop_fg[i].x, (int)s_hoop_fg[i].y, TILE_SIZE, TILE_SIZE);
        } else {
            png_draw_sprite_transform(s_tileset, &r, (int)s_hoop_fg[i].x, (int)s_hoop_fg[i].y, TILE_SIZE, TILE_SIZE, xf);
        }
    }
    s_hoop_fg_count = 0;
}

// Public control to get order: background -> ball -> ring foreground
static int s_ring_fg_defer = 0;
void level_set_ring_fg_defer(int on) { s_ring_fg_defer = on ? 1 : 0; }
void level_flush_ring_foreground(void) { hoop_fg_flush(); }

// Функции для инкапсуляции доступа к тайловому атласу
texture_t* level_get_tileset(void) { return s_tileset; }
int level_get_tiles_per_row(void) { return s_tiles_per_row; }


/* --- Валидация масок коллизий ---
 * КРИТИЧНО: Маски коллизий в level_masks.inc жестко привязаны к тайлам 12x12 пикселя.
 * Эти маски определяют попикселльные формы коллизий для:
 * - Треугольных рамп (TRI_TILE_DATA[12][12])  
 * - Коллизий маленького мяча (SMALL_BALL_DATA[12][12])
 * 
 * При изменении TILE_SIZE все маски должны быть пересозданы в новом разрешении.
 */
#if TILE_SIZE != 12
#error "TILE_SIZE должен быть 12: маски коллизий в level_masks.inc привязаны к размеру 12x12 пикселя"
#endif
#include "level_masks.inc"



Level g_level;

// --- Текстуры/атлас (перенесены в начало файла) ---


// --- Вспомогательное: единожды загрузить атлас ---
static void level_load_tileset_once(void) {
    if (s_tileset) return;
    s_tileset = png_load_texture_vram(TILESET_PATH);
    if (s_tileset && s_tileset->width > 0) {
        s_tiles_per_row = s_tileset->actual_width / TILE_SIZE; // 12 px на тайл
    } else {
        s_tiles_per_row = 0;
    }
}

// --- Загрузка уровня из файла ---
int level_load_from_file(const char* filename) {
    FILE* file = util_open_file(filename, "rb");
    if (!file) return 0;

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (fileSize < 8) { fclose(file); return 0; }

    unsigned char* buffer = (unsigned char*)malloc((size_t)fileSize);
    if (!buffer) { fclose(file); return 0; }

    size_t bytesRead = fread(buffer, 1, (size_t)fileSize, file);
    fclose(file);
    if (bytesRead != (size_t)fileSize) { free(buffer); return 0; }

    int result = level_load_from_memory((const char*)buffer, (int)fileSize);
    free(buffer);
    return result;
}

// --- Загрузка уровня по номеру ---
int level_load_by_number(int levelNumber) {
    level_load_tileset_once();
    char filename[256];
    snprintf(filename, sizeof(filename), LEVEL_PATH_FORMAT, levelNumber);
    int result = level_load_from_file(filename);
    
    return result;
}

// --- Парсер из памяти ---
int level_load_from_memory(const char* levelData, int dataSize) {
    if (!levelData || dataSize < 8) return 0;
    memset(&g_level, 0, sizeof(Level));

    const unsigned char* data = (const unsigned char*)levelData;
    int offset = 0;

    int startX_tiles   = data[offset++];
    int startY_tiles   = data[offset++];
    g_level.ballSize   = data[offset++];
    g_level.exitPosX   = data[offset++];
    g_level.exitPosY   = data[offset++];
    g_level.totalRings = data[offset++];
    g_level.width      = data[offset++];
    g_level.height     = data[offset++];

    if (g_level.width <= 0 || g_level.height <= 0 ||
        g_level.width  > MAX_LEVEL_WIDTH || g_level.height > MAX_LEVEL_HEIGHT) {
        return 0;
    }

    int mapBytes = g_level.width * g_level.height;
    if (offset + mapBytes > dataSize) return 0;

    int start_half = (g_level.ballSize == BALL_SIZE_SMALL) ? HALF_NORMAL_SIZE : HALF_ENLARGED_SIZE;
    g_level.startPosX = startX_tiles * TILE_SIZE + start_half;
    g_level.startPosY = startY_tiles * TILE_SIZE + start_half;
    g_level.startTileX = startX_tiles;
    g_level.startTileY = startY_tiles;

    for (int y = 0; y < g_level.height; ++y) {
        for (int x = 0; x < g_level.width; ++x) {
            g_level.tileMap[y][x] = data[offset++];
        }
    }

    // Загружаем движущиеся объекты (если есть)
    g_level.numMovingObjects = 0;
    if (dataSize - offset >= 1) {
        int numMoveObj = data[offset++];
        if (numMoveObj > 0 && numMoveObj <= MAX_MOVING_OBJECTS) {
            // Проверяем, что хватает данных для всех объектов (каждый = 8 байт)
            int requiredBytes = numMoveObj * 8;
            if (dataSize - offset >= requiredBytes) {
                g_level.numMovingObjects = numMoveObj;
                
                // Читаем данные каждого движущегося объекта
                for (int i = 0; i < numMoveObj; ++i) {
                    MovingObject* obj = &g_level.movingObjects[i];
                    
                    // Читаем topLeft, botRight, direction, startOffset
                    obj->topLeft[0] = data[offset++];      // X
                    obj->topLeft[1] = data[offset++];      // Y
                    obj->botRight[0] = data[offset++];     // X  
                    obj->botRight[1] = data[offset++];     // Y
                    obj->direction[0] = (short)(signed char)data[offset++];    // X direction (знаковый int8_t -> short)
                    obj->direction[1] = (short)(signed char)data[offset++];    // Y direction (знаковый int8_t -> short)  
                    obj->offset[0] = data[offset++];       // Start X offset
                    obj->offset[1] = data[offset++];       // Start Y offset
                }
            }
        }
    }

    return 1;
}


// --- Доступ к тайлам ---
int level_get_tile_at(int tileX, int tileY) {
    if (tileX < 0 || tileX >= g_level.width || tileY < 0 || tileY >= g_level.height) {
        return 1; // вне карты считаем стеной
    }
    return g_level.tileMap[tileY][tileX];
}

// --- Проверка коллизии с треугольной рампой ---

// --- Новые функции рендеринга ---

// Рендер EXIT тайла: фоновые полоски (plain pass)
static void render_exit_tile_plain(int tile_id, float destX, float destY, int worldTileX, int worldTileY) {
    if (tile_id == 9) { // EXIT - новая логика по якорю exitPos
        int local_x = worldTileX - g_level.exitPosX;
        int local_y = worldTileY - g_level.exitPosY;

        if (local_x == 0 && local_y == 0) {
            // Только левый верхний тайл рисует фон для всей области 2x2
            u32 background = BACKGROUND_COLOUR;
            u32 first_stripe = BACKGROUND_COLOUR;
            u32 light_stripe = EXIT_LIGHT_STRIPE_COLOUR;
            u32 dark_stripe = EXIT_DARK_STRIPE_COLOUR;
            u32 fourth_stripe = EXIT_FOURTH_STRIPE_COLOUR;

            float area_width = 2.0f * TILE_SIZE;
            float area_height = 2.0f * TILE_SIZE;

            graphics_draw_rect(destX, destY, area_width, area_height, background);
            graphics_draw_rect(destX + EXIT_STRIPE_1_X, destY, (float)EXIT_STRIPE_1_WIDTH, area_height, first_stripe);
            graphics_draw_rect(destX + EXIT_STRIPE_2_X, destY, (float)EXIT_STRIPE_2_WIDTH, area_height, light_stripe);
            graphics_draw_rect(destX + EXIT_STRIPE_3_X, destY, (float)EXIT_STRIPE_3_WIDTH, area_height, dark_stripe);
            graphics_draw_rect(destX + EXIT_STRIPE_4_X, destY, (float)EXIT_STRIPE_4_WIDTH, area_height, fourth_stripe);
        }
    } else if (tile_id == 10) {
        graphics_draw_rect(destX, destY, (float)TILE_SIZE, (float)TILE_SIZE, WATER_COLOUR);
    } else {
        graphics_draw_rect(destX, destY, (float)TILE_SIZE, (float)TILE_SIZE, 0xFF888888);
    }
}

// Рендер EXIT тайла: спрайты двери (textured pass)
static void render_exit_tile_textured(int tile_id, float destX, float destY, int worldTileX, int worldTileY) {
    if (tile_id != 9) return;

    int local_x = worldTileX - g_level.exitPosX;
    int local_y = worldTileY - g_level.exitPosY;

    if (local_x != 0 || local_y != 0) return;
    if ((uint32_t)tile_id >= tile_meta_count()) return;
    if (!s_tileset || s_tiles_per_row <= 0) return;

    const TileMeta* t = &tile_meta_db()[tile_id];
    const int col = t->sprite_index % s_tiles_per_row;
    const int row = t->sprite_index / s_tiles_per_row;

    int srcX = col * TILE_SIZE;
    int srcY = row * TILE_SIZE;
    sprite_rect_t r = png_create_sprite_rect(s_tileset, srcX, srcY, TILE_SIZE, TILE_SIZE);

    int animationOffset = game_exit_anim_offset();
    float doorX = destX;
    float doorY = destY - animationOffset;
    float areaTop = destY;

    if (doorY < areaTop) {
        float clipOffset = areaTop - doorY;
        if (clipOffset < TILE_SIZE) {
            float visibleHeight = TILE_SIZE - clipOffset;
            sprite_rect_t clipped_r = png_create_sprite_rect(s_tileset, srcX, srcY + (int)clipOffset, TILE_SIZE, (int)visibleHeight);

            png_draw_sprite(s_tileset, &clipped_r, (int)doorX, (int)areaTop, TILE_SIZE, (int)visibleHeight);
            png_draw_sprite_transform(s_tileset, &clipped_r, (int)(doorX + TILE_SIZE), (int)areaTop, TILE_SIZE, (int)visibleHeight, PNG_TRANSFORM_FLIP_X);

            if (doorY + TILE_SIZE >= areaTop) {
                png_draw_sprite_transform(s_tileset, &r, (int)doorX, (int)(doorY + TILE_SIZE), TILE_SIZE, TILE_SIZE, PNG_TRANSFORM_FLIP_Y);
                png_draw_sprite_transform(s_tileset, &r, (int)(doorX + TILE_SIZE), (int)(doorY + TILE_SIZE), TILE_SIZE, TILE_SIZE, PNG_TRANSFORM_ROT_180);
            }
        }
    } else {
        png_draw_sprite(s_tileset, &r, (int)doorX, (int)doorY, TILE_SIZE, TILE_SIZE);
        png_draw_sprite_transform(s_tileset, &r, (int)(doorX + TILE_SIZE), (int)doorY, TILE_SIZE, TILE_SIZE, PNG_TRANSFORM_FLIP_X);
        png_draw_sprite_transform(s_tileset, &r, (int)doorX, (int)(doorY + TILE_SIZE), TILE_SIZE, TILE_SIZE, PNG_TRANSFORM_FLIP_Y);
        png_draw_sprite_transform(s_tileset, &r, (int)(doorX + TILE_SIZE), (int)(doorY + TILE_SIZE), TILE_SIZE, TILE_SIZE, PNG_TRANSFORM_ROT_180);
    }
}

// Рендер движущихся шипов: фон тайла (plain pass)
static void render_moving_spikes_tile_plain(int tileX, int tileY, float destX, float destY) {
    unsigned int tile = (unsigned short)g_level.tileMap[tileY][tileX];
    bool is_water = (tile & TILE_FLAG_WATER) ? true : false;
    u32 bg_color = is_water ? WATER_COLOUR : BACKGROUND_COLOUR;
    graphics_draw_rect(destX, destY, (float)TILE_SIZE, (float)TILE_SIZE, bg_color);
}

// Рендер движущихся шипов: спрайты (textured pass)
static void render_moving_spikes_tile_textured(int tileX, int tileY, float destX, float destY) {
    int objIndex = level_find_moving_object_at(tileX, tileY);
    if (objIndex == -1) return;
    if (!s_tileset || s_tiles_per_row <= 0) return;
    if (10 >= tile_meta_count()) return;

    MovingObject* obj = &g_level.movingObjects[objIndex];
    int relTileX = tileX - obj->topLeft[0];
    int relTileY = tileY - obj->topLeft[1];

    float offsetX = (float)obj->offset[0] - (float)(relTileX * TILE_SIZE);
    float offsetY = (float)obj->offset[1] - (float)(relTileY * TILE_SIZE);

    if (offsetX > -3 * TILE_SIZE && offsetX < TILE_SIZE && offsetY > -3 * TILE_SIZE && offsetY < TILE_SIZE) {
        const TileMeta* t = &tile_meta_db()[10];
        int col = t->sprite_index % s_tiles_per_row;
        int row = t->sprite_index / s_tiles_per_row;

        for (int dy = 0; dy < 2; dy++) {
            for (int dx = 0; dx < 2; dx++) {
                float spriteX = destX + offsetX + (float)(dx * TILE_SIZE);
                float spriteY = destY + offsetY + (float)(dy * TILE_SIZE);

                if (spriteX < destX + TILE_SIZE && spriteX + TILE_SIZE > destX &&
                    spriteY < destY + TILE_SIZE && spriteY + TILE_SIZE > destY) {

                    int srcX = col * TILE_SIZE;
                    int srcY = row * TILE_SIZE;
                    sprite_rect_t r = png_create_sprite_rect(s_tileset, srcX, srcY, TILE_SIZE, TILE_SIZE);

                    png_transform_t xf = PNG_TRANSFORM_IDENTITY;
                    if (dx == 1 && dy == 0) xf = PNG_TRANSFORM_FLIP_X;
                    if (dx == 0 && dy == 1) xf = PNG_TRANSFORM_FLIP_Y;
                    if (dx == 1 && dy == 1) xf = PNG_TRANSFORM_ROT_180;

                    png_draw_sprite_transform(s_tileset, &r, (int)spriteX, (int)spriteY, TILE_SIZE, TILE_SIZE, xf);
                }
            }
        }
    }
}

// REMOVED: render_dual_sprite_tile - was deprecated and unused

// Рендер кольца-обруча: фон тайла (plain pass)
static void render_hoop_tile_plain(float destX, float destY, int flags) {
    u32 bg_color = (flags & TILE_FLAG_WATER) ? WATER_COLOUR : BACKGROUND_COLOUR;
    graphics_draw_rect(destX, destY, (float)TILE_SIZE, (float)TILE_SIZE, bg_color);
}

// Рендер кольца-обруча: спрайты и очередь foreground (textured pass)
static void render_hoop_tile_textured(const TileMeta* t, float destX, float destY, int tileID) {
    if (!s_tileset || s_tiles_per_row <= 0) return;
    if (tileID < 13 || tileID > 28 || !is_sprite_valid(t->sprite_index)) return;

    int col = t->sprite_index % s_tiles_per_row;
    int row = t->sprite_index / s_tiles_per_row;
    int srcX = col * TILE_SIZE;
    int srcY = row * TILE_SIZE;

    sprite_rect_t r = png_create_sprite_rect(s_tileset, srcX, srcY, TILE_SIZE, TILE_SIZE);

    TileTransform bg_transform = (t->orientation == ORIENT_VERT_TOP) ? TF_ROT_270_FLIP_X :
                                (t->orientation == ORIENT_VERT_BOTTOM) ? TF_ROT_270_FLIP_XY :
                                (t->orientation == ORIENT_HORIZ_LEFT) ? TF_FLIP_Y :
                                (t->orientation == ORIENT_HORIZ_RIGHT) ? TF_FLIP_XY : TF_NONE;
    png_transform_t xf = map_tf_to_png(bg_transform);

    png_draw_sprite_transform(s_tileset, &r, (int)destX, (int)destY, TILE_SIZE, TILE_SIZE, xf);

    TileTransform fg_transform = (t->orientation == ORIENT_VERT_TOP) ? TF_ROT_270 :
                                (t->orientation == ORIENT_VERT_BOTTOM) ? TF_ROT_270_FLIP_Y :
                                (t->orientation == ORIENT_HORIZ_RIGHT) ? TF_FLIP_X : TF_NONE;
    hoop_fg_push(t->sprite_index, destX, destY, fg_transform);
}

// --- Рендер видимой области (ОБНОВЛЕНО) ---
// ВАЖНО: После вызова функция оставляет произвольное текстурное состояние.
// Состояние текстур управляется централизованно через graphics.c
void level_render_visible_area(int cameraX, int cameraY, int screenWidth, int screenHeight) {
    
    hoop_fg_clear();
    if (g_level.width <= 0 || g_level.height <= 0) return;

    int startTileX = cameraX / TILE_SIZE;
    int endTileX   = (cameraX + screenWidth  - 1) / TILE_SIZE;
    int startTileY = cameraY / TILE_SIZE;
    int endTileY   = (cameraY + screenHeight - 1) / TILE_SIZE;

    if (startTileX < 0) startTileX = 0;
    if (startTileY < 0) startTileY = 0;
    if (endTileX >= g_level.width)   endTileX = g_level.width - 1;
    if (endTileY >= g_level.height)  endTileY = g_level.height - 1;

    // Pass 1: plain фон (минимизируем переключения режима)
    graphics_begin_plain();

    for (int y = startTileY; y <= endTileY; ++y) {
        for (int x = startTileX; x <= endTileX; ++x) {
            unsigned int tile = (unsigned short)g_level.tileMap[y][x];
            bool is_water = (tile & TILE_FLAG_WATER) ? true : false;
            int original_tile_flags = tile & TILE_FLAGS_MASK;

            if (is_water) {
                tile = tile & ~TILE_FLAG_WATER;
            }

            int tile_id = tile & TILE_ID_MASK;
            int tile_flags = original_tile_flags;

            float screenX = (float)(x * TILE_SIZE - cameraX);
            float screenY = (float)(y * TILE_SIZE - cameraY);

            if (tile_id == 0) {
                u32 bg_color = is_water ? WATER_COLOUR : BACKGROUND_COLOUR;
                graphics_draw_rect(screenX, screenY, (float)TILE_SIZE, (float)TILE_SIZE, bg_color);
                continue;
            }

            if (tile_id < 0 || tile_id >= (int)tile_meta_count()) continue;

            if (!s_tileset || s_tiles_per_row <= 0) {
                graphics_draw_rect(screenX, screenY, (float)TILE_SIZE, (float)TILE_SIZE, 0xFF444444);
                continue;
            }

            const TileMeta* t = &tile_meta_db()[tile_id];

            if (tile_id == 9) {
                if (is_water) {
                    graphics_draw_rect(screenX, screenY, (float)TILE_SIZE, (float)TILE_SIZE, WATER_COLOUR);
                }
                render_exit_tile_plain(tile_id, screenX, screenY, x, y);
                continue;
            } else if (tile_id == 10) {
                render_moving_spikes_tile_plain(x, y, screenX, screenY);
                continue;
            } else if (t->render_type & RENDER_COMPOSITE) {
                if (is_water) {
                    graphics_draw_rect(screenX, screenY, (float)TILE_SIZE, (float)TILE_SIZE, WATER_COLOUR);
                }
                render_exit_tile_plain(tile_id, screenX, screenY, x, y);
                continue;
            } else if (t->render_type & RENDER_HOOP) {
                render_hoop_tile_plain(screenX, screenY, tile_flags);
                continue;
            }

            if (is_water) {
                graphics_draw_rect(screenX, screenY, (float)TILE_SIZE, (float)TILE_SIZE, WATER_COLOUR);
            }

            if (!is_sprite_valid(t->sprite_index)) {
                graphics_draw_rect(screenX, screenY, (float)TILE_SIZE, (float)TILE_SIZE, 0xFF444444);
            }
        }
    }

    // Pass 2: текстуры (спрайты)
    graphics_begin_textured();

    for (int y = startTileY; y <= endTileY; ++y) {
        for (int x = startTileX; x <= endTileX; ++x) {
            unsigned int tile = (unsigned short)g_level.tileMap[y][x];
            bool is_water = (tile & TILE_FLAG_WATER) ? true : false;

            if (is_water) {
                tile = tile & ~TILE_FLAG_WATER;
            }

            int tile_id = tile & TILE_ID_MASK;

            if (tile_id == 0) continue;
            if (tile_id < 0 || tile_id >= (int)tile_meta_count()) continue;
            if (!s_tileset || s_tiles_per_row <= 0) continue;

            const TileMeta* t = &tile_meta_db()[tile_id];

            float screenX = (float)(x * TILE_SIZE - cameraX);
            float screenY = (float)(y * TILE_SIZE - cameraY);

            if (tile_id == 9) {
                render_exit_tile_textured(tile_id, screenX, screenY, x, y);
                continue;
            } else if (tile_id == 10) {
                render_moving_spikes_tile_textured(x, y, screenX, screenY);
                continue;
            } else if (t->render_type & RENDER_COMPOSITE) {
                render_exit_tile_textured(tile_id, screenX, screenY, x, y);
                continue;
            } else if (t->render_type & RENDER_HOOP) {
                render_hoop_tile_textured(t, screenX, screenY, tile_id);
                continue;
            }

            if (is_sprite_valid(t->sprite_index)) {
                int col = t->sprite_index % s_tiles_per_row;
                int row = t->sprite_index / s_tiles_per_row;
                int srcX = col * TILE_SIZE;
                int srcY = row * TILE_SIZE;

                png_transform_t xf = map_tf_to_png(t->transform);
                sprite_rect_t r = png_create_sprite_rect(s_tileset, srcX, srcY, TILE_SIZE, TILE_SIZE);
                if (xf == PNG_TRANSFORM_IDENTITY) {
                    png_draw_sprite(s_tileset, &r, (int)screenX, (int)screenY, TILE_SIZE, TILE_SIZE);
                } else {
                    png_draw_sprite_transform(s_tileset, &r, (int)screenX, (int)screenY, TILE_SIZE, TILE_SIZE, xf);
                }
            }
        }
    }
    if (!s_ring_fg_defer) hoop_fg_flush();
}

// --- Функции для движущихся объектов ---

// Обновление позиций движущихся объектов (логика из Java updateMovingSpikeObj)
// ПРИМЕЧАНИЕ: Делитель 30 FPS теперь управляется из game.c
void level_update_moving_objects(void) {
    
    for (int i = 0; i < g_level.numMovingObjects; ++i) {
        MovingObject* obj = &g_level.movingObjects[i];
        
        // Обновляем X offset
        obj->offset[0] += obj->direction[0];
        
        // Вычисляем границы движения (в пикселях)
        int maxOffsetX = (obj->botRight[0] - obj->topLeft[0] - 2) * TILE_SIZE;
        int maxOffsetY = (obj->botRight[1] - obj->topLeft[1] - 2) * TILE_SIZE;
        
        // Проверяем границы по X и отражаем направление при достижении
        if (obj->offset[0] <= 0) {
            obj->offset[0] = 0;
            obj->direction[0] = -obj->direction[0];
        } else if (obj->offset[0] >= maxOffsetX) {
            obj->offset[0] = (short)maxOffsetX;
            obj->direction[0] = -obj->direction[0];
        }
        
        // Обновляем Y offset  
        obj->offset[1] += obj->direction[1];
        
        // Проверяем границы по Y и отражаем направление при достижении
        if (obj->offset[1] <= 0) {
            obj->offset[1] = 0;
            obj->direction[1] = -obj->direction[1];
        } else if (obj->offset[1] >= maxOffsetY) {
            obj->offset[1] = (short)maxOffsetY;
            obj->direction[1] = -obj->direction[1];
        }
    }
}

// Поиск движущегося объекта в данном тайле (аналог findSpikeIndex)
int level_find_moving_object_at(int tileX, int tileY) {
    for (int i = 0; i < g_level.numMovingObjects; ++i) {
        MovingObject* obj = &g_level.movingObjects[i];
        
        // Проверяем, входит ли тайл в область движущегося объекта
        if (obj->topLeft[0] <= tileX && obj->botRight[0] > tileX &&
            obj->topLeft[1] <= tileY && obj->botRight[1] > tileY) {
            return i;
        }
    }
    return -1;  // Не найдено
}

MovingObject* level_get_moving_object(int index) {
    if (index >= 0 && index < g_level.numMovingObjects) {
        return &g_level.movingObjects[index];
    }
    return NULL;
}

// ============================================================================
// ОПЕРАЦИИ С ТАЙЛАМИ КАРТЫ (для событийной системы)
// ============================================================================

// Временно удаляем - переместим в начало файла

// Получить ID тайла (без флагов)
uint8_t level_get_id(int tx, int ty) {
    if (tx < 0 || tx >= g_level.width || ty < 0 || ty >= g_level.height) {
        return 0; // За пределами карты - пустой тайл
    }
    return (uint8_t)(g_level.tileMap[ty][tx] & TILE_CLEAN_MASK);
}

// Установить ID тайла (сохраняя флаги)
void level_set_id(int tx, int ty, uint8_t id) {
    if (tx >= 0 && tx < g_level.width && ty >= 0 && ty < g_level.height) {
        short old_tile = g_level.tileMap[ty][tx];
        short flags = old_tile & ~TILE_ID_MASK;  // Сохраняем все флаги
        g_level.tileMap[ty][tx] = flags | (id & TILE_ID_MASK);  // Объединяем с новым ID
    }
}


// Деактивировать старый чекпоинт (меняем ID с 7 на 8)
void level_deactivate_old_checkpoint(void) {
    if (s_respawn_x >= 0 && s_respawn_x < g_level.width && s_respawn_y >= 0 && s_respawn_y < g_level.height) {
        uint8_t currentId = level_get_id(s_respawn_x, s_respawn_y);
        if (currentId == TILE_CHECKPOINT) {  // Если это активный чекпоинт (7)
            level_set_id(s_respawn_x, s_respawn_y, TILE_CHECKPOINT_ON);  // Меняем на неактивный (8)
        }
    }
}

// Активировать чекпоинт ((id&0x7F)|0x88) - соответствует Java: tileMap[paramInt1][paramInt2] = 136
void level_mark_checkpoint_active(int tx, int ty) {
    if (tx >= 0 && tx < g_level.width && ty >= 0 && ty < g_level.height) {
        uint8_t id = level_get_id(tx, ty);
        id = TILE_CHECKPOINT_ON; // Просто 8, без dirty бита (ТЕСТ)
        level_set_id(tx, ty, id);
    }
}

// Установить новую точку респауна (соответствует Java: setRespawn)
// ВАЖНО: Эта функция только сохраняет координаты. Управление визуальным состоянием
// чекпоинтов (деактивация старого, активация нового) должно выполняться вызывающим кодом.
// См. game_set_respawn() для полного алгоритма активации чекпоинта.
void level_set_respawn(int tx, int ty) {
    s_respawn_x = tx;
    s_respawn_y = ty;
}

// Получить текущую позицию респауна
void level_get_respawn(int* tx, int* ty) {
    if (tx) *tx = s_respawn_x;
    if (ty) *ty = s_respawn_y;
}

// --- Cleanup function for resource deallocation ---
void level_cleanup(void) {
    if (s_tileset) {
        png_free_texture(s_tileset);
        s_tileset = NULL;
        s_tiles_per_row = 0;
    }
}
