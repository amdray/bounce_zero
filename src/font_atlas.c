#include <pspkernel.h>
#include "font_atlas.h"
#include "font9_atlas.h"
#include "font12_atlas.h"
#include "font23_atlas.h"

static const FontAtlas* get_atlas_by_height(int font_height) {
    if (font_height == 23) {
        return font23_get_atlas();
    }
    if (font_height == 12) {
        return font12_get_atlas();
    }
    return font9_get_atlas();
}

const FontAtlas* font_atlas_get(int font_height) {
    return get_atlas_by_height(font_height);
}

const FontGlyph* font_atlas_lookup(const FontAtlas* atlas, u32 codepoint) {
    if (!atlas) return NULL;

    u32 cp = codepoint;
    if (cp < atlas->range_start || cp > atlas->range_end) {
        cp = atlas->default_cp;
    }

    const FontGlyph* g = &atlas->glyphs[cp - atlas->range_start];
    if (g->w == 0) {
        g = &atlas->glyphs[atlas->default_cp - atlas->range_start];
    }
    return g;
}

static void writeback_atlas(const FontAtlas* atlas) {
    if (!atlas) return;
    for (int i = 0; i < atlas->page_count; i++) {
        const texture_t* tex = &atlas->pages[i];
        size_t size = (size_t)(tex->width * tex->height) / 2; // T4
        sceKernelDcacheWritebackRange(tex->data, size);
    }
}

void font_atlas_prepare(void) {
    writeback_atlas(font9_get_atlas());
    writeback_atlas(font12_get_atlas());
    writeback_atlas(font23_get_atlas());
}
