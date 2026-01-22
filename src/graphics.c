#include "graphics.h"
#include "font9.h"
#include "font12.h"
#include "font23.h"
#include "font24.h"
#include "font_atlas.h"
#include "types.h"  // Для SCREEN_WIDTH/SCREEN_HEIGHT
#include "png.h"    // Для texture_t
#include <pspgu.h>
#include <pspdisplay.h>
#include <pspkernel.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// Используем константы из graphics.h для VRAM буферов
// (локальные #define заменены на глобальные константы)

// Размеры буферов (байты на пиксель)
#define FRAMEBUFFER_BPP 4  // GU_PSM_8888 = 4 байта на пиксель

// Увеличенный буфер команд GU для более сложной отрисовки
#define GU_CMD_LIST_SIZE (256 * 1024)  // 256KB буфер команд GU (стандартный размер)
static char s_list[GU_CMD_LIST_SIZE] __attribute__((aligned(64)));


// VRAM буферы - вычисляются динамически
static void* s_draw_buffer;
static void* s_disp_buffer;

// Единое управление состоянием текстур
// ИНВАРИАНТ: кадр начинается в plain-режиме (текстуры выключены)
static int s_texturing_enabled = 0;


// Sprite batching система (по образцу pspsdk/samples/gu/sprite)
#define MAX_SPRITES_PER_BATCH 128
typedef struct {
    float u, v;
    u32 color;
    float x, y, z;
} BatchVertex;

typedef struct {
    BatchVertex vertices[MAX_SPRITES_PER_BATCH * 2]; // GU_SPRITES = 2 вершины на спрайт
    texture_t* current_texture;
    int count;
} SpriteBatch;

static SpriteBatch s_batch;

// CLUT для T4 шрифтов: index 0 = прозрачный, index 1 = белый
static u32 s_font_clut[16] __attribute__((aligned(16)));

// Forward declarations
static void batch_init(void);

static void font_set_clut_fixed(void) {
    s_font_clut[0] = 0x00000000; // прозрачный
    s_font_clut[1] = 0xFFFFFFFF; // белый
    for (int i = 2; i < 16; i++) {
        s_font_clut[i] = 0x00000000;
    }

    sceKernelDcacheWritebackRange(s_font_clut, sizeof(s_font_clut));
    sceGuClutMode(GU_PSM_8888, 0, 0xFF, 0);
    sceGuClutLoad(2, s_font_clut); // 16 цветов = 2 блока
}

// Простые примитивы без текстур
typedef struct {
    short x, y, z;
} Vertex2D;

void graphics_init(void) {
    // Размеры буферов
    const size_t framebuffer_size = VRAM_BUFFER_WIDTH * VRAM_BUFFER_HEIGHT * FRAMEBUFFER_BPP;
    
    // Буферы как смещения в VRAM (не абсолютные адреса!)
    s_draw_buffer = (void*)0;                    // смещение 0
    s_disp_buffer = (void*)framebuffer_size;     // смещение после первого FB
    
    sceGuInit();

    sceGuStart(GU_DIRECT, s_list);
    sceGuDrawBuffer(GU_PSM_8888, s_draw_buffer, VRAM_BUFFER_WIDTH);
    sceGuDispBuffer(SCREEN_WIDTH, SCREEN_HEIGHT, s_disp_buffer, VRAM_BUFFER_WIDTH);
    // Depth buffer не выделяем - не нужен для 2D рендера

    sceGuOffset(2048 - (SCREEN_WIDTH / 2), 2048 - (SCREEN_HEIGHT / 2));
    sceGuViewport(2048, 2048, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Включаем альфа-блендинг внутри активного списка
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);

    // Очистить буферы перед показом (избежать мусора)
    // Цвет уже в ABGR формате, передаём напрямую
    sceGuClearColor(0);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
    
    // Инвариант: начинаем кадр в plain-режиме (текстуры выключены)
    s_texturing_enabled = 0;
    
    // Подготовить atlas-данные для GU (T4, чтение из RAM)
    font_atlas_prepare();

    // Инициализация batch системы
    batch_init();
}

void graphics_start_frame(void) {
    sceGuStart(GU_DIRECT, s_list);
}

void graphics_set_scissor_fullscreen(void) {
    sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

void graphics_clear(u32 color) {
    sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuClearColor(color);
    sceGuClear(GU_COLOR_BUFFER_BIT);
}


void graphics_draw_rect(float x, float y, float w, float h, u32 color) {
    // Не трогаем GU_TEXTURE_2D / GU_BLEND — вариант B
    Vertex2D* v = (Vertex2D*)sceGuGetMemory(2 * sizeof(Vertex2D));

    v[0].x = (short)x;
    v[0].y = (short)y;
    v[0].z = 0;

    v[1].x = (short)(x + w);
    v[1].y = (short)(y + h);
    v[1].z = 0;

    // sceGuColor принимает цвет в том же ABGR формате что и graphics_clear()
    sceGuColor(color);
    sceGuDrawArray(GU_SPRITES, GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);
}


// Декодирование UTF-8 в Unicode codepoint
int utf8_decode_to_codepoint(const char* str, int* bytes_read) {
    unsigned char c = (unsigned char)str[0];
    *bytes_read = 1;

    if (c < 0x80) {
        return c;
    }

    if ((c & 0xE0) == 0xC0) {
        unsigned char c2 = (unsigned char)str[1];
        if (c2 == '\0' || (c2 & 0xC0) != 0x80) {
            return 0x20;
        }
        *bytes_read = 2;
        return ((c & 0x1F) << 6) | (c2 & 0x3F);
    }

    if ((c & 0xF0) == 0xE0) {
        unsigned char c2 = (unsigned char)str[1];
        unsigned char c3 = (unsigned char)str[2];
        if (c2 == '\0' || c3 == '\0') {
            return 0x20;
        }
        if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) {
            return 0x20;
        }
        *bytes_read = 3;
        return ((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
    }

    if ((c & 0xF8) == 0xF0) {
        unsigned char c2 = (unsigned char)str[1];
        unsigned char c3 = (unsigned char)str[2];
        unsigned char c4 = (unsigned char)str[3];
        if (c2 == '\0' || c3 == '\0' || c4 == '\0') {
            return 0x20;
        }
        if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80 || (c4 & 0xC0) != 0x80) {
            return 0x20;
        }
        *bytes_read = 4;
        return ((c & 0x07) << 18) | ((c2 & 0x3F) << 12) | ((c3 & 0x3F) << 6) | (c4 & 0x3F);
    }

    return 0x20;
}

void graphics_draw_text(float x, float y, const char* text, u32 color, int font_height) {
    if (!text) return;
    int cur_x = (int)x;
    int base_y = (int)y;
    int i = 0;
    int spacing;

    if (font_height == 23) {
        spacing = FONT23_SPACING;
    } else if (font_height == 12) {
        spacing = FONT12_SPACING;
    } else {
        spacing = FONT9_SPACING;
    }

    const FontAtlas* atlas = font_atlas_get(font_height);
    if (!atlas) return;

    int current_page = -1;
    texture_t* current_tex = NULL;

    // Изолируем текст от возможного прошлого батча
    graphics_flush_batch();
    graphics_begin_textured();
    font_set_clut_fixed();
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuEnable(GU_ALPHA_TEST);
    sceGuAlphaFunc(GU_GREATER, 0, 0xFF);
    while (text[i] != '\0') {
        int bytes_read;
        int cp = utf8_decode_to_codepoint(&text[i], &bytes_read);
        const FontGlyph* glyph = font_atlas_lookup(atlas, (u32)cp);
        if (!glyph) break;

        int width = glyph->w;
        int height = glyph->h;

        if (width > 0) {
            if (glyph->page != (u8)current_page) {
                current_page = glyph->page;
                current_tex = (texture_t*)&atlas->pages[current_page];
                graphics_flush_batch();
                graphics_bind_texture(current_tex);
                font_set_clut_fixed();
                sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
            }

            float u1 = (float)glyph->x;
            float v1 = (float)glyph->y;
            float u2 = u1 + (float)width;
            float v2 = v1 + (float)height;
            graphics_batch_sprite_colored(u1, v1, u2, v2,
                                          (float)cur_x, (float)base_y,
                                          (float)width, (float)height, color);
        }

        cur_x += width + spacing;
        i += bytes_read;
    }

    graphics_flush_batch();
    sceGuDisable(GU_ALPHA_TEST);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    graphics_begin_plain();
}

int graphics_measure_text(const char* text, int font_height) {
    if (!text) return 0;
    int width = 0;
    int i = 0;

    int spacing;
    if (font_height == 23) {
        spacing = FONT23_SPACING;
    } else if (font_height == 12) {
        spacing = FONT12_SPACING;
    } else {
        spacing = FONT9_SPACING;
    }

    const FontAtlas* atlas = font_atlas_get(font_height);
    if (!atlas) return 0;

    while (text[i] != '\0') {
        int bytes_read;
        int cp = utf8_decode_to_codepoint(&text[i], &bytes_read);
        const FontGlyph* glyph = font_atlas_lookup(atlas, (u32)cp);
        int glyph_width = glyph ? glyph->w : 0;
        width += glyph_width;

        // Добавить spacing только между символами (не после последнего)
        int next_i = i + bytes_read;
        if (text[next_i] != '\0') {
            width += spacing;
        }

        i = next_i;
    }
    return width;
}

void graphics_draw_number(float x, float y, int number, u32 color) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", number);

    int cur_x = (int)x;
    int cur_y = (int)y;

    for (int i = 0; buffer[i] != '\0'; i++) {
        if (buffer[i] >= '0' && buffer[i] <= '9') {
            int digit = buffer[i] - '0';
            const Glyph24* glyph = font24_get_digit(digit);

            for (int row = 0; row < FONT24_HEIGHT; row++) {
                for (int col = 0; col < glyph->width; col++) {
                    if (glyph->row[row] & (1 << (15 - col))) {
                        graphics_draw_rect(cur_x + col, cur_y + row, 1, 1, color);
                    }
                }
            }

            cur_x += glyph->width + FONT24_SPACING;
        } else if (buffer[i] == '-') {
            graphics_draw_rect(cur_x, cur_y + FONT24_HEIGHT / 2, 8, 2, color);
            cur_x += 10;
        }
    }
}

int graphics_measure_number(int number) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", number);

    int width = 0;

    for (int i = 0; buffer[i] != '\0'; i++) {
        if (buffer[i] >= '0' && buffer[i] <= '9') {
            int digit = buffer[i] - '0';
            const Glyph24* glyph = font24_get_digit(digit);
            width += glyph->width;

            if (buffer[i + 1] != '\0') {
                width += FONT24_SPACING;
            }
        } else if (buffer[i] == '-') {
            width += 10;
        }
    }

    return width;
}

// Единое управление состоянием текстур
void graphics_set_texturing(int enabled) {
    if (enabled && !s_texturing_enabled) {
        sceGuEnable(GU_TEXTURE_2D);
        s_texturing_enabled = 1;
    } else if (!enabled && s_texturing_enabled) {
        sceGuDisable(GU_TEXTURE_2D);
        s_texturing_enabled = 0;
    }
}

void graphics_begin_plain(void) {
    graphics_flush_batch(); // Завершить накопленные спрайты перед переключением в plain
    graphics_set_texturing(0);
    // Блендинг уже включён в graphics_init(); ничего менять не нужно
}

void graphics_begin_textured(void) {
    graphics_set_texturing(1);                 // включает GU_TEXTURE_2D при необходимости
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA); // отключаем модуляцию цветом вершины
}

// Получить текущее состояние текстур (для оптимизации в level.c)
int graphics_get_texturing_state(void) {
    return s_texturing_enabled; // 0=plain, 1=textured
}

void graphics_end_frame(void) {
    graphics_flush_batch(); // Завершить все накопленные спрайты перед концом кадра
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

void graphics_shutdown(void) {
    graphics_flush_batch(); // Убедимся что все спрайты отрисованы перед завершением
    
    sceGuDisplay(GU_FALSE);
    sceGuTerm();
}

// =================== SPRITE BATCHING СИСТЕМА ===================

// Инициализация batch системы
static void batch_init(void) {
    s_batch.current_texture = NULL;
    s_batch.count = 0;
}

// Отправить накопленные спрайты на рендер
void graphics_flush_batch(void) {
    if (s_batch.count <= 0) return;
    
    if (s_batch.current_texture) {
        graphics_bind_texture(s_batch.current_texture);
        // НЕ дублируем TexScale/Offset — они уже выставляются внутри graphics_bind_texture()
    }
    
    // «Железобезопасная» альтернатива - копируем в GE память
    const int vcount = s_batch.count * 2;
    BatchVertex* vtx = (BatchVertex*)sceGuGetMemory(vcount * sizeof(BatchVertex));
    if (!vtx) { 
        s_batch.count = 0; 
        return; 
    }
    memcpy(vtx, s_batch.vertices, vcount * sizeof(BatchVertex));
    
    // Отрисовать всю пачку одним вызовом
    sceGuDrawArray(GU_SPRITES, 
                   GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, 
                   vcount, 0, vtx);
    
    s_batch.count = 0; // Очистить batch для новых спрайтов
}

// Привязать текстуру для batch'а (flush если текстура изменилась)
void graphics_bind_texture(texture_t* tex) {
    if (!tex) return;
    
    // Если текстура изменилась - flush накопленные спрайты
    if (s_batch.current_texture != tex) {
        graphics_flush_batch();
        
        s_batch.current_texture = tex;
        
        // Привязать новую текстуру (как в pspsdk/samples/gu/sprite)
        sceGuTexMode(tex->format, 0, 0, 0);
        // stride = width корректно: текстуры расширены до POT, данные непрерывны
        sceGuTexImage(0, tex->width, tex->height, tex->width, tex->data);
        sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
        sceGuTexFilter(GU_NEAREST, GU_NEAREST);
        sceGuTexWrap(GU_CLAMP, GU_CLAMP);
        
    }
}

// Добавить спрайт в batch (не отрисовывает сразу!)
void graphics_batch_sprite(float u1, float v1, float u2, float v2, 
                          float x, float y, float w, float h) {
    graphics_batch_sprite_colored(u1, v1, u2, v2, x, y, w, h, 0xFFFFFFFF);
}

void graphics_batch_sprite_colored(float u1, float v1, float u2, float v2,
                          float x, float y, float w, float h, u32 color) {
    // Flush если batch переполнен
    if (s_batch.count >= MAX_SPRITES_PER_BATCH) {
        graphics_flush_batch();
    }
    
    int idx = s_batch.count * 2; // 2 вершины на спрайт
    
    // Первая вершина (левый верхний угол)
    s_batch.vertices[idx].u = u1;
    s_batch.vertices[idx].v = v1;
    s_batch.vertices[idx].color = color;
    s_batch.vertices[idx].x = x;
    s_batch.vertices[idx].y = y;
    s_batch.vertices[idx].z = 0.0f;
    
    // Вторая вершина (правый нижний угол)
    s_batch.vertices[idx + 1].u = u2;
    s_batch.vertices[idx + 1].v = v2;
    s_batch.vertices[idx + 1].color = color;
    s_batch.vertices[idx + 1].x = x + w;
    s_batch.vertices[idx + 1].y = y + h;
    s_batch.vertices[idx + 1].z = 0.0f;
    
    s_batch.count++;
}
