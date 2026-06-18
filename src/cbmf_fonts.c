#include "cbmf_fonts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FONT_COUNT 5

#define FONT_IDX_9  0
#define FONT_IDX_12 1
#define FONT_IDX_16 2
#define FONT_IDX_23 3
#define FONT_IDX_24 4

static const char *s_font_paths[FONT_COUNT] = {
    "fonts/0012-0002.cbmf",
    "fonts/0015-0064.cbmf",
    "fonts/0019-0128.cbmf",
    "fonts/0026-0002.cbmf",
    "fonts/0027-0032.cbmf",
};

static const int s_font_heights[FONT_COUNT] = { 9, 12, 16, 23, 24 };

/* slot_w чётный — обязательно для выравнивания байт T4 (sx >> 1) */
static const uint16_t s_slot_w[FONT_COUNT] = { 12, 14, 18, 24, 16 };
static const uint16_t s_slot_h[FONT_COUNT] = {  9, 12, 16, 23, 24 };

/* T4 кэш-текстуры. font23 использует 256×128 чтобы вместить 50 слотов */
#define TEX_W    128
#define TEX_H    128
#define TEX23_W  256
#define TEX_BYTES   (TEX_W   * TEX_H / 2)
#define TEX23_BYTES (TEX23_W * TEX_H / 2)

static uint8_t s_tex9 [TEX_BYTES]   __attribute__((aligned(16)));
static uint8_t s_tex12[TEX_BYTES]   __attribute__((aligned(16)));
static uint8_t s_tex16[TEX_BYTES]   __attribute__((aligned(16)));
static uint8_t s_tex23[TEX23_BYTES] __attribute__((aligned(16)));
static uint8_t s_tex24[TEX_BYTES]   __attribute__((aligned(16)));

static uint8_t *s_tex_bufs[FONT_COUNT] = {
    s_tex9, s_tex12, s_tex16, s_tex23, s_tex24
};

static const uint16_t s_tex_w[FONT_COUNT]  = { TEX_W, TEX_W, TEX_W, TEX23_W, TEX_W };
static const uint16_t s_tex_h[FONT_COUNT]  = { TEX_H, TEX_H, TEX_H, TEX_H,   TEX_H };
static const uint16_t s_tex_pitch[FONT_COUNT] = { TEX_W, TEX_W, TEX_W, TEX23_W, TEX_W };

/* CLUT: по 16 записей на каждый шрифт, 16-байтное выравнивание */
static uint32_t s_clut9 [16] __attribute__((aligned(16)));
static uint32_t s_clut12[16] __attribute__((aligned(16)));
static uint32_t s_clut16[16] __attribute__((aligned(16)));
static uint32_t s_clut23[16] __attribute__((aligned(16)));
static uint32_t s_clut24[16] __attribute__((aligned(16)));

static uint32_t *s_cluts[FONT_COUNT] = {
    s_clut9, s_clut12, s_clut16, s_clut23, s_clut24
};

/* Таблицы слотов: slots_x * slots_y для каждого шрифта */
/* font9:  128/12=10, 128/9 =14 → 140 слотов */
/* font12: 128/14= 9, 128/12=10 →  90 слотов */
/* font16: 128/18= 7, 128/16= 8 →  56 слотов */
/* font23: 256/24=10, 128/23= 5 →  50 слотов (256-wide tex) */
/* font24: 128/16= 8, 128/24= 5 →  40 слотов */
static uint32_t s_slots9 [140];
static uint32_t s_slots12[ 90];
static uint32_t s_slots16[ 56];
static uint32_t s_slots23[ 50];
static uint32_t s_slots24[ 40];

static uint32_t *s_slot_bufs[FONT_COUNT] = {
    s_slots9, s_slots12, s_slots16, s_slots23, s_slots24
};
static const uint16_t s_slot_counts[FONT_COUNT] = { 140, 90, 56, 50, 40 };

static void        *s_font_data[FONT_COUNT];
static CbmfFont     s_fonts[FONT_COUNT];
static CbmfPspRenderer s_renderers[FONT_COUNT];

static int load_and_mount(int idx) {
    FILE *f = fopen(s_font_paths[idx], "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) { fclose(f); return -1; }

    void *buf = malloc((size_t)size);
    if (!buf) { fclose(f); return -1; }

    if ((long)fread(buf, 1, (size_t)size, f) != size) {
        free(buf);
        fclose(f);
        return -1;
    }
    fclose(f);

    int rc = cbmf_mount(&s_fonts[idx], buf, (size_t)size);
    if (rc != CBMF_OK) { free(buf); return rc; }

    s_font_data[idx] = buf;
    return CBMF_OK;
}

int cbmf_fonts_init(void) {
    memset(s_font_data, 0, sizeof(s_font_data));

    for (int i = 0; i < FONT_COUNT; i++) {
        int rc = load_and_mount(i);
        if (rc != CBMF_OK) return rc;

        CbmfPspRendererDesc desc;
        memset(&desc, 0, sizeof(desc));
        desc.texture_pixels  = s_tex_bufs[i];
        desc.texture_width   = s_tex_w[i];
        desc.texture_height  = s_tex_h[i];
        desc.texture_pitch   = s_tex_pitch[i];
        desc.slot_width      = s_slot_w[i];
        desc.slot_height     = s_slot_h[i];
        desc.slot_codepoints = s_slot_bufs[i];
        desc.slot_count      = s_slot_counts[i];
        desc.clut            = s_cluts[i];

        rc = cbmf_psp_init(&s_renderers[i], &s_fonts[i], &desc);
        if (rc != CBMF_PSP_OK) return rc;
    }

    return 0;
}

void cbmf_fonts_shutdown(void) {
    for (int i = 0; i < FONT_COUNT; i++) {
        if (s_font_data[i]) {
            free(s_font_data[i]);
            s_font_data[i] = NULL;
        }
    }
}

static int height_to_idx(int font_height) {
    for (int i = 0; i < FONT_COUNT; i++) {
        if (s_font_heights[i] == font_height) return i;
    }
    return FONT_IDX_9;
}

CbmfPspRenderer *cbmf_fonts_get_renderer(int font_height) {
    return &s_renderers[height_to_idx(font_height)];
}

const CbmfFont *cbmf_fonts_get_font(int font_height) {
    return &s_fonts[height_to_idx(font_height)];
}
