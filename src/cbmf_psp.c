#include "cbmf_psp.h"

#include <string.h>
#include <pspgu.h>
#include <pspkernel.h>

#define CBMF_EMPTY_SLOT 0xFFFFFFFFu

static int psp_decode_utf8_one(const char **cursor, uint32_t *out_cp) {
    const uint8_t *s;
    uint32_t cp;
    uint8_t b0;

    if (!cursor || !*cursor || !out_cp) {
        return CBMF_ERR_NULL;
    }

    s = (const uint8_t *)(*cursor);
    b0 = s[0];
    if (b0 == 0) {
        return CBMF_ERR_NOT_FOUND;
    }

    if (b0 < 0x80u) {
        *out_cp = (uint32_t)b0;
        *cursor += 1;
        return CBMF_OK;
    }

    if ((b0 & 0xE0u) == 0xC0u) {
        uint8_t b1 = s[1];
        if ((b1 & 0xC0u) != 0x80u) {
            return CBMF_ERR_BAD_UTF8;
        }
        cp = ((uint32_t)(b0 & 0x1Fu) << 6) | (uint32_t)(b1 & 0x3Fu);
        if (cp < 0x80u) {
            return CBMF_ERR_BAD_UTF8;
        }
        *out_cp = cp;
        *cursor += 2;
        return CBMF_OK;
    }

    if ((b0 & 0xF0u) == 0xE0u) {
        uint8_t b1 = s[1];
        uint8_t b2 = s[2];
        if ((b1 & 0xC0u) != 0x80u || (b2 & 0xC0u) != 0x80u) {
            return CBMF_ERR_BAD_UTF8;
        }
        cp = ((uint32_t)(b0 & 0x0Fu) << 12) |
             ((uint32_t)(b1 & 0x3Fu) << 6) |
             (uint32_t)(b2 & 0x3Fu);
        if (cp < 0x800u || (cp >= 0xD800u && cp <= 0xDFFFu)) {
            return CBMF_ERR_BAD_UTF8;
        }
        *out_cp = cp;
        *cursor += 3;
        return CBMF_OK;
    }

    if ((b0 & 0xF8u) == 0xF0u) {
        uint8_t b1 = s[1];
        uint8_t b2 = s[2];
        uint8_t b3 = s[3];
        if ((b1 & 0xC0u) != 0x80u ||
            (b2 & 0xC0u) != 0x80u ||
            (b3 & 0xC0u) != 0x80u) {
            return CBMF_ERR_BAD_UTF8;
        }
        cp = ((uint32_t)(b0 & 0x07u) << 18) |
             ((uint32_t)(b1 & 0x3Fu) << 12) |
             ((uint32_t)(b2 & 0x3Fu) << 6) |
             (uint32_t)(b3 & 0x3Fu);
        if (cp < 0x10000u || cp > 0x10FFFFu) {
            return CBMF_ERR_BAD_UTF8;
        }
        *out_cp = cp;
        *cursor += 4;
        return CBMF_OK;
    }

    return CBMF_ERR_BAD_UTF8;
}

static int psp_get_glyph_with_fallback(
    const CbmfFont *font,
    uint32_t codepoint,
    CbmfGlyphView *out
) {
    int rc = cbmf_get_glyph(font, codepoint, out);
    if (rc == CBMF_OK) {
        return CBMF_OK;
    }
    if (rc != CBMF_ERR_NOT_FOUND || !font) {
        return rc;
    }
    if (font->fallback_codepoint == codepoint) {
        return CBMF_ERR_NOT_FOUND;
    }
    return cbmf_get_glyph(font, font->fallback_codepoint, out);
}

static int psp_find_cached_slot(
    const CbmfPspRenderer *renderer,
    uint32_t codepoint,
    uint16_t *out_slot
) {
    uint16_t i;
    if (!renderer || !out_slot) {
        return CBMF_PSP_ERR_NULL;
    }
    for (i = 0; i < renderer->slot_count; ++i) {
        if (renderer->slot_codepoints[i] == codepoint) {
            *out_slot = i;
            return CBMF_PSP_OK;
        }
    }
    return CBMF_PSP_ERR_NOT_FOUND;
}

static void psp_clear_slot(CbmfPspRenderer *renderer, uint16_t slot) {
    uint16_t sx = (uint16_t)((slot % renderer->slots_x) * renderer->slot_width);
    uint16_t sy = (uint16_t)((slot / renderer->slots_x) * renderer->slot_height);
    uint16_t pitch_bytes = (uint16_t)(renderer->texture_pitch >> 1);
    uint16_t row_bytes   = (uint16_t)((renderer->slot_width + 1u) >> 1);
    uint16_t y;
    for (y = 0; y < renderer->slot_height; ++y) {
        uint8_t *dst = renderer->texture_pixels
                     + ((uint32_t)(sy + y) * pitch_bytes) + (sx >> 1);
        memset(dst, 0, row_bytes);
    }
}

static int psp_unpack_glyph_to_slot(
    CbmfPspRenderer *renderer,
    const CbmfGlyphView *glyph,
    uint16_t slot
) {
    if (!renderer || !glyph) {
        return CBMF_PSP_ERR_NULL;
    }
    if (glyph->width > renderer->slot_width || glyph->height > renderer->slot_height) {
        return CBMF_PSP_ERR_GLYPH_TOO_LARGE;
    }

    psp_clear_slot(renderer, slot);

    uint16_t sx = (uint16_t)((slot % renderer->slots_x) * renderer->slot_width);
    uint16_t sy = (uint16_t)((slot / renderer->slots_x) * renderer->slot_height);
    uint16_t pitch_bytes = (uint16_t)(renderer->texture_pitch >> 1);
    uint16_t y;

    for (y = 0; y < glyph->height; ++y) {
        const uint8_t *src = glyph->bitmap + (uint32_t)y * glyph->row_bytes;
        uint8_t       *dst = renderer->texture_pixels
                           + ((uint32_t)(sy + y) * pitch_bytes) + (sx >> 1);
        memcpy(dst, src, glyph->row_bytes);
    }

    renderer->slot_codepoints[slot] = glyph->codepoint;
    renderer->texture_dirty = true;
    return CBMF_PSP_OK;
}

static int psp_get_cached_slot(
    CbmfPspRenderer *renderer,
    const CbmfGlyphView *glyph,
    uint16_t *out_slot
) {
    int rc;
    uint16_t slot;
    if (!renderer || !glyph || !out_slot) {
        return CBMF_PSP_ERR_NULL;
    }

    rc = psp_find_cached_slot(renderer, glyph->codepoint, out_slot);
    if (rc == CBMF_PSP_OK) {
        return CBMF_PSP_OK;
    }

    slot = renderer->next_slot;
    renderer->next_slot = (uint16_t)((renderer->next_slot + 1u) % renderer->slot_count);
    rc = psp_unpack_glyph_to_slot(renderer, glyph, slot);
    if (rc != CBMF_PSP_OK) {
        return rc;
    }
    *out_slot = slot;
    return CBMF_PSP_OK;
}

static int psp_map_core_error(int rc) {
    if (rc == CBMF_OK) {
        return CBMF_PSP_OK;
    }
    if (rc == CBMF_ERR_BAD_UTF8) {
        return CBMF_PSP_ERR_BAD_UTF8;
    }
    if (rc == CBMF_ERR_NOT_FOUND) {
        return CBMF_PSP_ERR_NOT_FOUND;
    }
    if (rc == CBMF_ERR_NULL) {
        return CBMF_PSP_ERR_NULL;
    }
    return CBMF_PSP_ERR_BAD_DESC;
}

/*
 * Establish the complete GU state required by this renderer for one text
 * draw. The immutable CLUT turns the T4 texture into an alpha mask; vertex
 * color supplies the requested text color. CLUT state belongs to the GU
 * command stream, so it is rebound for every draw call.
 */
static void psp_bind_render_state(CbmfPspRenderer *renderer) {
    sceGuClutMode(GU_PSM_8888, 0, 0xFF, 0);
    sceGuClutLoad(2, renderer->clut);

    sceGuTexMode(GU_PSM_T4, 0, 0, 0);
    sceGuTexImage(0,
        renderer->texture_width, renderer->texture_height,
        renderer->texture_pitch,
        renderer->texture_pixels);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
}

static void psp_sync_texture_if_dirty(CbmfPspRenderer *renderer) {
    uint16_t pitch_bytes;

    if (!renderer->texture_dirty) {
        return;
    }

    pitch_bytes = (uint16_t)(renderer->texture_pitch >> 1);
    sceKernelDcacheWritebackRange(
        renderer->texture_pixels,
        (size_t)renderer->texture_height * pitch_bytes
    );
    sceGuTexFlush();
    renderer->texture_dirty = false;
}

static void psp_emit_glyph_quad(
    CbmfPspRenderer *renderer,
    uint16_t slot,
    int x,
    int y,
    const CbmfGlyphView *glyph,
    uint32_t color
) {
    typedef struct {
        uint16_t u, v;
        uint32_t color;
        int16_t x, y, z;
        uint16_t pad;
    } GlyphVert;
    GlyphVert *v;
    uint16_t u0, v0, u1, v1;

    u0 = (uint16_t)((slot % renderer->slots_x) * renderer->slot_width);
    v0 = (uint16_t)((slot / renderer->slots_x) * renderer->slot_height);
    u1 = (uint16_t)(u0 + glyph->width);
    v1 = (uint16_t)(v0 + glyph->height);

    v = sceGuGetMemory(2 * sizeof(GlyphVert));
    v[0].u = u0; v[0].v = v0;
    v[0].color = color;
    v[0].x = (int16_t)x;               v[0].y = (int16_t)y;               v[0].z = 0;
    v[0].pad = 0;
    v[1].u = u1; v[1].v = v1;
    v[1].color = color;
    v[1].x = (int16_t)(x + glyph->width); v[1].y = (int16_t)(y + glyph->height); v[1].z = 0;
    v[1].pad = 0;

    sceGuDrawArray(GU_SPRITES,
        GU_TEXTURE_16BIT | GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
        2, 0, v);
}

int cbmf_psp_init(
    CbmfPspRenderer *renderer,
    const CbmfFont *font,
    const CbmfPspRendererDesc *desc
) {
    uint16_t slots_x;
    uint16_t slots_y;
    uint32_t capacity;

    if (!renderer || !font || !desc) {
        return CBMF_PSP_ERR_NULL;
    }
    if (!desc->texture_pixels || !desc->slot_codepoints || !desc->clut) {
        return CBMF_PSP_ERR_NULL;
    }
    if (desc->texture_width == 0u || desc->texture_height == 0u ||
        desc->texture_pitch == 0u || desc->slot_width == 0u ||
        desc->slot_height == 0u || desc->slot_count == 0u) {
        return CBMF_PSP_ERR_BAD_DESC;
    }
    if (desc->texture_pitch < desc->texture_width) {
        return CBMF_PSP_ERR_BAD_DESC;
    }

    slots_x = (uint16_t)(desc->texture_width / desc->slot_width);
    slots_y = (uint16_t)(desc->texture_height / desc->slot_height);
    if (slots_x == 0u || slots_y == 0u) {
        return CBMF_PSP_ERR_CACHE_TOO_SMALL;
    }

    capacity = (uint32_t)slots_x * (uint32_t)slots_y;
    if ((uint32_t)desc->slot_count > capacity) {
        return CBMF_PSP_ERR_BAD_DESC;
    }

    memset(renderer, 0, sizeof(*renderer));
    renderer->font = font;
    renderer->texture_pixels = desc->texture_pixels;
    renderer->texture_width = desc->texture_width;
    renderer->texture_height = desc->texture_height;
    renderer->texture_pitch = desc->texture_pitch;
    renderer->slot_width = desc->slot_width;
    renderer->slot_height = desc->slot_height;
    renderer->slots_x = slots_x;
    renderer->slots_y = slots_y;
    renderer->slot_count = desc->slot_count;
    renderer->slot_codepoints = desc->slot_codepoints;
    renderer->clut = desc->clut;
    renderer->texture_dirty = false;

    memset(renderer->clut, 0, 16 * sizeof(uint32_t));
    renderer->clut[1] = 0xFFFFFFFFu;
    sceKernelDcacheWritebackRange(renderer->clut, 16 * sizeof(uint32_t));

    cbmf_psp_reset_cache(renderer);
    return CBMF_PSP_OK;
}

void cbmf_psp_reset_cache(CbmfPspRenderer *renderer) {
    uint16_t i;
    if (!renderer || !renderer->slot_codepoints) {
        return;
    }
    for (i = 0; i < renderer->slot_count; ++i) {
        renderer->slot_codepoints[i] = CBMF_EMPTY_SLOT;
    }
    renderer->next_slot = 0u;
}

int cbmf_psp_draw_utf8(
    CbmfPspRenderer *renderer,
    int x,
    int y,
    const char *text,
    uint32_t color
) {
    const char *p;
    int pen_x;

    if (!renderer || !renderer->font || !text) {
        return CBMF_PSP_ERR_NULL;
    }
    if (*text == '\0') {
        return CBMF_PSP_OK;
    }

    psp_bind_render_state(renderer);

    p = text;
    pen_x = x;
    while (*p) {
        uint32_t cp;
        CbmfGlyphView glyph;
        uint16_t slot;
        int rc;

        rc = psp_decode_utf8_one(&p, &cp);
        if (rc != CBMF_OK) {
            return psp_map_core_error(rc);
        }

        rc = psp_get_glyph_with_fallback(renderer->font, cp, &glyph);
        if (rc != CBMF_OK) {
            return psp_map_core_error(rc);
        }

        if (glyph.width > renderer->slot_width || glyph.height > renderer->slot_height) {
            return CBMF_PSP_ERR_GLYPH_TOO_LARGE;
        }

        rc = psp_get_cached_slot(renderer, &glyph, &slot);
        if (rc != CBMF_PSP_OK) {
            return rc;
        }

        psp_sync_texture_if_dirty(renderer);

        psp_emit_glyph_quad(
            renderer,
            slot,
            pen_x + (int)glyph.offset_x,
            y + (int)glyph.offset_y,
            &glyph,
            color
        );
        pen_x += (int)glyph.advance_x;
    }

    return CBMF_PSP_OK;
}
