// png.c - Загрузка PNG файлов
#include "png.h"
#include "types.h"     // Для util_open_file
#include "graphics.h"  // Для новой batch системы
#include <pspgu.h>
#include <pspkernel.h>
#include <pspge.h>     // Для sceGeEdramGetAddr/GetSize
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

// Включаем stb_image для загрузки PNG
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO  // Используем свои функции чтения файлов
#define STBI_ONLY_PNG  // Только PNG поддержка
#include "stb_image.h"

// =================== VRAM ALLOCATION ===================

// Статический указатель для VRAM аллокации (начинаем после framebuffer'ов)
#define FRAMEBUFFER_BPP 4
static unsigned int staticVramOffset = (VRAM_BUFFER_WIDTH * VRAM_BUFFER_HEIGHT * FRAMEBUFFER_BPP) * 2;


// Helper функция для ограничения значения границами
static inline float clamp_bounds(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

// Вычисление размера памяти для текстуры
static unsigned int getTextureMemorySize(unsigned int width, unsigned int height, unsigned int psm) {
    switch (psm) {
        case GU_PSM_T4:
            return (width * height) >> 1;
        case GU_PSM_T8:
            return width * height;
        case GU_PSM_5650:
        case GU_PSM_5551:
        case GU_PSM_4444:
        case GU_PSM_T16:
            return 2 * width * height;
        case GU_PSM_8888:
        case GU_PSM_T32:
            return 4 * width * height;
        default:
            return 0;
    }
}

// Аллокация буфера в VRAM (возвращает смещение)
static void* getStaticVramBuffer(unsigned int width, unsigned int height, unsigned int psm) {
    unsigned int memSize = getTextureMemorySize(width, height, psm);
    unsigned int vramSize = sceGeEdramGetSize(); // ~2 MiB на PSP
    if (staticVramOffset + memSize > vramSize) {
        return NULL; // VRAM переполнена
    }

    // Выравнивание по 16 байт для PSP GU
    staticVramOffset = (staticVramOffset + 15) & ~15;

    void* result = (void*)staticVramOffset;
    staticVramOffset += memSize;
    return result;
}

// Аллокация текстуры в VRAM (возвращает полный адрес)
static void* getStaticVramTexture(unsigned int width, unsigned int height, unsigned int psm) {
    void* vramOffset = getStaticVramBuffer(width, height, psm);
    if (!vramOffset) return NULL;
    return (void*)(((unsigned int)vramOffset) + ((unsigned int)sceGeEdramGetAddr()));
}


/*
 * ПЛАТФОРМО-ЗАВИСИМЫЕ ДЕТАЛИ TEXTURE_T:
 * - Для GU_PSM_8888 данные хранятся как RGBA байты в памяти
 * - На PSP (little-endian) это соответствует ABGR словам
 * - КРИТИЧНО: после изменения tex->data требуется sceKernelDcacheWritebackRange()
 *   перед следующим sceGuTexImage() для синхронизации кэша
 */

// Структура для передачи данных файла в stb_image
typedef struct {
    unsigned char* data;
    int size;
    int pos;
} MemoryBuffer;

// Структура вершины для текстурированных примитивов
typedef struct {
    float u, v;
    float x, y, z;
} TextureVertex;

// Функция чтения для stb_image
static int read_func(void* user, char* data, int size) {
    MemoryBuffer* buffer = (MemoryBuffer*)user;
    if (buffer->pos + size > buffer->size) {
        size = buffer->size - buffer->pos;
    }
    if (size <= 0) return 0;
    
    memcpy(data, buffer->data + buffer->pos, size);
    buffer->pos += size;
    return size;
}

// Функция пропуска для stb_image
static void skip_func(void* user, int n) {
    MemoryBuffer* buffer = (MemoryBuffer*)user;
    buffer->pos += n;
    if (buffer->pos > buffer->size) {
        buffer->pos = buffer->size;
    }
}

// Функция проверки конца файла для stb_image
static int eof_func(void* user) {
    MemoryBuffer* buffer = (MemoryBuffer*)user;
    return buffer->pos >= buffer->size;
}


// Загрузка PNG в VRAM
texture_t* png_load_texture_vram(const char* path) {
    // Открываем файл с fallback путями
    FILE* file = util_open_file(path, "rb");
    
    if (!file) {
        return NULL;
    }
    
    // Читаем весь файл в память
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(file);
        return NULL;
    }
    
    unsigned char* file_data = (unsigned char*)malloc(file_size);
    if (!file_data) {
        fclose(file);
        return NULL;
    }
    
    size_t read_bytes = fread(file_data, 1, file_size, file);
    fclose(file);
    
    if (read_bytes != (size_t)file_size) {
        free(file_data);
        return NULL;
    }
    
    MemoryBuffer buffer;
    buffer.data = file_data;
    buffer.size = file_size;
    buffer.pos = 0;
    
    stbi_io_callbacks callbacks;
    callbacks.read = read_func;
    callbacks.skip = skip_func;
    callbacks.eof = eof_func;
    
    int width, height, channels;
    unsigned char* image_data = stbi_load_from_callbacks(&callbacks, &buffer, &width, &height, &channels, 4);
    
    free(file_data);
    
    if (!image_data) {
        return NULL;
    }
    
    // Создаем текстуру с unified cleanup для предотвращения утечек памяти
    texture_t* tex = NULL;
    unsigned char* tex_data = NULL;
    
    tex = (texture_t*)malloc(sizeof(texture_t));
    if (!tex) {
        goto cleanup;
    }
    
    // Округляем размеры до степени двойки (требование PSP)
    int tex_width = 1;
    int tex_height = 1;
    while (tex_width < width) tex_width <<= 1;
    while (tex_height < height) tex_height <<= 1;
    
    tex->width = tex_width;
    tex->height = tex_height;
    tex->actual_width = width;
    tex->actual_height = height;
    tex->format = GU_PSM_8888;
    
    // Выделяем память для текстуры - сначала VRAM, при нехватке fallback в RAM
    size_t tex_size = (size_t)tex_width * (size_t)tex_height * 4;
    int use_vram = 1;
    tex_data = getStaticVramTexture(tex_width, tex_height, GU_PSM_8888);
    if (!tex_data) {
        // VRAM переполнена - fallback в RAM  
        use_vram = 0;
        tex_data = memalign(16, tex_size);
        // Диагностика: можно раскомментировать при отладке
        // printf("VRAM full, fallback to RAM (%dx%d)\n", tex_width, tex_height);
    }
    if (!tex_data) {
        goto cleanup;
    }
    
    tex->data = tex_data;
    tex->is_vram = use_vram;
    
    // Очищаем память текстуры
    memset(tex->data, 0, tex_size);
    
    // Копируем данные изображения в текстуру (строками быстрее, чем по пикселям)
    unsigned char* dest = (unsigned char*)tex->data;
    size_t row_bytes = (size_t)width * 4;
    for (int y = 0; y < height; y++) {
        memcpy(dest + (size_t)y * (size_t)tex_width * 4,
               image_data + (size_t)y * row_bytes,
               row_bytes);
    }
    
    // Cache writeback для RAM текстур
    if (!tex->is_vram) {
        sceKernelDcacheWritebackRange(tex->data, tex_size);
    }
    
    // Успешное завершение - освобождаем только image_data
    stbi_image_free(image_data);
    return tex;
    
cleanup:
    // Unified cleanup - освобождаем все ресурсы при ошибке
    if (tex_data && !use_vram) {
        // Только RAM освобождаем через free()
        free(tex_data);
    }
    if (tex) {
        free(tex);
    }
    if (image_data) {
        stbi_image_free(image_data);
    }
    return NULL;
}

// Загрузка PNG в VRAM

sprite_rect_t png_create_sprite_rect(texture_t* tex, int x, int y, int w, int h) {
    sprite_rect_t rect;
    if (!tex || tex->width == 0 || tex->height == 0) {
        rect.u = 0.0f;
        rect.v = 0.0f;
        rect.width = 1.0f;
        rect.height = 1.0f;
        return rect;
    }
    
    rect.u = (float)x / (float)tex->width;
    rect.v = (float)y / (float)tex->height;
    rect.width  = (float)w / (float)tex->width;
    rect.height = (float)h / (float)tex->height;
    return rect;
}

void png_draw_sprite(texture_t* tex, sprite_rect_t* sprite, int x, int y, int w, int h) {
    if (!tex || !sprite || !tex->data) return;
    if (tex->width <= 0 || tex->height <= 0) return;
    if (w <= 0 || h <= 0) return;
    
    // Вычисляем UV координаты в пикселях (как раньше)
    float u1 = sprite->u * (float)tex->width;
    float v1 = sprite->v * (float)tex->height;
    float u2 = (sprite->u + sprite->width) * (float)tex->width;
    float v2 = (sprite->v + sprite->height) * (float)tex->height;
    
    // Используем исправленную batch систему (с копированием в GE память)
    graphics_bind_texture(tex);  // Автоматически flush'нет если текстура изменилась
    graphics_batch_sprite(u1, v1, u2, v2, (float)x, (float)y, (float)w, (float)h);  // Добавляем в batch
}

void png_free_texture(texture_t* tex) {
    if (!tex) return;
    if (tex->data && !tex->is_vram) {
        // Только RAM текстуры освобождаем через free()
        // VRAM управляется статически
        free(tex->data);
    }
    tex->data = NULL; // Безопасность от двойного освобождения
    free(tex);
}

void png_draw_sprite_uv4(texture_t* tex,
                         float u_tl, float v_tl,
                         float u_tr, float v_tr,
                         float u_bl, float v_bl,
                         float u_br, float v_br,
                         int x, int y, int w, int h)
{
    if (!tex || !tex->data) return;
    if (w <= 0 || h <= 0) return;

    TextureVertex* v = (TextureVertex*)sceGuGetMemory(4 * sizeof(TextureVertex));
    if (!v) return;

    // Clamp to texture bounds
    float tw = (float)tex->width, th = (float)tex->height;
    
    u_tl = clamp_bounds(u_tl, 0.0f, tw);
    v_tl = clamp_bounds(v_tl, 0.0f, th);
    u_tr = clamp_bounds(u_tr, 0.0f, tw);
    v_tr = clamp_bounds(v_tr, 0.0f, th);
    u_bl = clamp_bounds(u_bl, 0.0f, tw);
    v_bl = clamp_bounds(v_bl, 0.0f, th);
    u_br = clamp_bounds(u_br, 0.0f, tw);
    v_br = clamp_bounds(v_br, 0.0f, th);


    // Top-left
    v[0].u = u_tl; v[0].v = v_tl;
    v[0].x = (float)x;    v[0].y = (float)y;    v[0].z = 0.0f;
    // Top-right
    v[1].u = u_tr; v[1].v = v_tr;
    v[1].x = (float)(x + w); v[1].y = (float)y;   v[1].z = 0.0f;
    // Bottom-left
    v[2].u = u_bl; v[2].v = v_bl;
    v[2].x = (float)x;    v[2].y = (float)(y + h); v[2].z = 0.0f;
    // Bottom-right
    v[3].u = u_br; v[3].v = v_br;
    v[3].x = (float)(x + w); v[3].y = (float)(y + h); v[3].z = 0.0f;

    // Синхронизация с batch системой
    graphics_flush_batch();
    graphics_begin_textured();
    graphics_bind_texture(tex);

    // Draw as triangle strip: TL, TR, BL, BR
    sceGuDrawArray(GU_TRIANGLE_STRIP,
                   GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_2D,
                   4, 0, v);
}

void png_draw_sprite_transform(texture_t* tex, sprite_rect_t* sprite,
                           int x, int y, int w, int h,
                           png_transform_t transform)
{
    if (!tex || !sprite) return;

    // Базовые границы UV в пикселях
    const float u1 = sprite->u * (float)tex->width;
    const float v1 = sprite->v * (float)tex->height;
    const float u2 = (sprite->u + sprite->width)  * (float)tex->width;
    const float v2 = (sprite->v + sprite->height) * (float)tex->height;

    // Исходные углы (TL, TR, BL, BR)
    float U[4] = { u1, u2, u1, u2 };
    float V[4] = { v1, v1, v2, v2 };

    // Раскладываем enum в (rot, fx, fy)
    int rot = 0, fx = 0, fy = 0; // rot: 0,1,2,3 (по часовой)
    switch (transform) {
        default:
        case PNG_TRANSFORM_IDENTITY: break;
        case PNG_TRANSFORM_ROT_90:   rot = 1; break;
        case PNG_TRANSFORM_ROT_180:  rot = 2; break;
        case PNG_TRANSFORM_ROT_270:  rot = 3; break;
        case PNG_TRANSFORM_FLIP_X:   fx  = 1; break;
        case PNG_TRANSFORM_FLIP_Y:   fy  = 1; break;
        case PNG_TRANSFORM_ROT_270_FLIP_X:   rot = 3; fx = 1; break;
        case PNG_TRANSFORM_ROT_270_FLIP_Y:   rot = 3; fy = 1; break;
        case PNG_TRANSFORM_ROT_270_FLIP_XY:  rot = 3; fx = 1; fy = 1; break;
    }

    // Применяем ROT_N (перестановка углов по часовой)
    // Индексы: 0=TL, 1=TR, 2=BL, 3=BR
    int idx[4] = {0,1,2,3};
    for (int r = 0; r < rot; ++r) {
        int TL = idx[0], TR = idx[1], BL = idx[2], BR = idx[3];
        idx[0] = TR; // TL <- TR
        idx[1] = BR; // TR <- BR
        idx[2] = TL; // BL <- TL
        idx[3] = BL; // BR <- BL
    }

    // Затем FLIP_X: меняем местами TL<->TR, BL<->BR
    if (fx) {
        int t = idx[0]; idx[0] = idx[1]; idx[1] = t;
            t = idx[2]; idx[2] = idx[3]; idx[3] = t;
    }

    // Затем FLIP_Y: меняем местами TL<->BL, TR<->BR
    if (fy) {
        int t = idx[0]; idx[0] = idx[2]; idx[2] = t;
            t = idx[1]; idx[1] = idx[3]; idx[3] = t;
    }

    // Собираем конечные UV
    const float tl_u = U[idx[0]], tl_v = V[idx[0]];
    const float tr_u = U[idx[1]], tr_v = V[idx[1]];
    const float bl_u = U[idx[2]], bl_v = V[idx[2]];
    const float br_u = U[idx[3]], br_v = V[idx[3]];

    png_draw_sprite_uv4(tex, tl_u, tl_v, tr_u, tr_v, bl_u, bl_v, br_u, br_v,
                        x, y, w, h);
}
