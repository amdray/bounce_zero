#include "graphics.h"
#include "cbmf_fonts.h"
#include "types.h"
#include "png.h"
#include <pspgu.h>
#include <pspdisplay.h>
#include <pspkernel.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
    uint16_t u, v;
    u32 color;
    short x, y, z;
    short pad;
} BatchVertex;

typedef struct {
    BatchVertex vertices[MAX_SPRITES_PER_BATCH * 2]; // GU_SPRITES = 2 вершины на спрайт
    texture_t* current_texture;
    int count;
} SpriteBatch;

static SpriteBatch s_batch;

// Forward declarations
static void batch_init(void);

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
    
    cbmf_fonts_init();

    // Инициализация batch системы
    batch_init();
}

void graphics_start_frame(void) {
    sceGuStart(GU_DIRECT, s_list);
    s_batch.current_texture = NULL;
}

void graphics_set_scissor_fullscreen(void) {
    sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

void graphics_clear(u32 color) {
    sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuClearColor(color);
    sceGuClear(GU_COLOR_BUFFER_BIT);
}


void graphics_draw_rect(int x, int y, int w, int h, u32 color) {
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



void graphics_draw_text(int x, int y, const char* text, u32 color, int font_height) {
    if (!text) return;
    CbmfPspRenderer *r = cbmf_fonts_get_renderer(font_height);
    if (!r) return;

    graphics_flush_batch();
    graphics_begin_textured();
    sceGuEnable(GU_ALPHA_TEST);
    sceGuAlphaFunc(GU_GREATER, 0, 0xFF);

    cbmf_psp_draw_utf8(r, x, y, text, color);

    sceGuDisable(GU_ALPHA_TEST);
    graphics_begin_plain();
}

int graphics_measure_text(const char* text, int font_height) {
    if (!text) return 0;
    const CbmfFont *f = cbmf_fonts_get_font(font_height);
    if (!f) return 0;
    int32_t w = 0;
    cbmf_text_width_utf8(f, text, &w);
    return (int)w;
}

void graphics_draw_number(int x, int y, int number, u32 color) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", number);
    graphics_draw_text(x, y, buf, color, 24);
}

int graphics_measure_number(int number) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", number);
    return graphics_measure_text(buf, 24);
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
    graphics_flush_batch();
    cbmf_fonts_shutdown();
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
        // Параметры текстуры выставляются внутри graphics_bind_texture().
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
                   GU_TEXTURE_16BIT|GU_COLOR_8888|GU_VERTEX_16BIT|GU_TRANSFORM_2D, 
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
void graphics_batch_sprite(int u1, int v1, int u2, int v2,
                          int x, int y, int w, int h) {
    graphics_batch_sprite_colored(u1, v1, u2, v2, x, y, w, h, 0xFFFFFFFF);
}

void graphics_batch_sprite_colored(int u1, int v1, int u2, int v2,
                          int x, int y, int w, int h, u32 color) {
    // Flush если batch переполнен
    if (s_batch.count >= MAX_SPRITES_PER_BATCH) {
        graphics_flush_batch();
    }
    
    int idx = s_batch.count * 2; // 2 вершины на спрайт
    
    // Первая вершина (левый верхний угол)
    s_batch.vertices[idx].u = (uint16_t)u1;
    s_batch.vertices[idx].v = (uint16_t)v1;
    s_batch.vertices[idx].color = color;
    s_batch.vertices[idx].x = (short)x;
    s_batch.vertices[idx].y = (short)y;
    s_batch.vertices[idx].z = 0;
    s_batch.vertices[idx].pad = 0;
    
    // Вторая вершина (правый нижний угол)
    s_batch.vertices[idx + 1].u = (uint16_t)u2;
    s_batch.vertices[idx + 1].v = (uint16_t)v2;
    s_batch.vertices[idx + 1].color = color;
    s_batch.vertices[idx + 1].x = (short)(x + w);
    s_batch.vertices[idx + 1].y = (short)(y + h);
    s_batch.vertices[idx + 1].z = 0;
    s_batch.vertices[idx + 1].pad = 0;
    
    s_batch.count++;
}
