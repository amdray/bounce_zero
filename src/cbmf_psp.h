#ifndef CBMF_PSP_H
#define CBMF_PSP_H

#include <stdint.h>
#include "cbmf.h"

/*
 * PSP backend contract:
 * - storage: CBMF T4 nibble (PSP native, low nibble = even x, high nibble = odd x)
 * - runtime cache texture format: GU_PSM_T4
 * - glyph color via CLUT[1]; CLUT[0] = 0x00000000 (transparent)
 * - no malloc/free
 * - cache slots are fixed grid cells
 * - cache eviction is round-robin
 */

#define CBMF_PSP_OK                    0
#define CBMF_PSP_ERR_NULL             -1
#define CBMF_PSP_ERR_BAD_DESC         -2
#define CBMF_PSP_ERR_CACHE_TOO_SMALL  -3
#define CBMF_PSP_ERR_GLYPH_TOO_LARGE  -4
#define CBMF_PSP_ERR_BAD_UTF8         -5
#define CBMF_PSP_ERR_NOT_FOUND        -6

typedef struct {
    /*
     * GU_PSM_T4 texture memory (1 byte = 2 pixels).
     * Provided by caller, 16-byte aligned.
     */
    uint8_t *texture_pixels;

    uint16_t texture_width;
    uint16_t texture_height;

    /*
     * Texture pitch in pixels (not bytes).
     * Byte stride = texture_pitch / 2.
     */
    uint16_t texture_pitch;

    /*
     * Fixed cache cell size.
     * Every cached glyph occupies one slot.
     */
    uint16_t slot_width;
    uint16_t slot_height;

    /*
     * Caller-provided slot table.
     * slot_codepoints[slot] stores cached codepoint.
     */
    uint32_t *slot_codepoints;
    uint16_t slot_count;

    /*
     * CLUT buffer: 16 entries, GU_PSM_8888.
     * Must be 16-byte aligned (sceGuClutLoad requirement).
     * Caller allocates: uint32_t clut[16] __attribute__((aligned(16)));
     */
    uint32_t *clut;
} CbmfPspRendererDesc;

typedef struct {
    const CbmfFont *font;

    uint8_t *texture_pixels;

    uint16_t texture_width;
    uint16_t texture_height;
    uint16_t texture_pitch;

    uint16_t slot_width;
    uint16_t slot_height;

    uint16_t slots_x;
    uint16_t slots_y;
    uint16_t slot_count;

    uint32_t *slot_codepoints;

    uint32_t *clut;
    uint32_t current_color;

    /*
     * Round-robin eviction cursor.
     */
    uint16_t next_slot;
} CbmfPspRenderer;

int cbmf_psp_init(
    CbmfPspRenderer *renderer,
    const CbmfFont *font,
    const CbmfPspRendererDesc *desc
);

void cbmf_psp_reset_cache(
    CbmfPspRenderer *renderer
);

int cbmf_psp_draw_utf8(
    CbmfPspRenderer *renderer,
    int x,
    int y,
    const char *text,
    uint32_t color
);

#endif
