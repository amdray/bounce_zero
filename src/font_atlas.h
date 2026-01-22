#ifndef FONT_ATLAS_H
#define FONT_ATLAS_H

#include <psptypes.h>
#include "png.h"

// Glyph metadata for atlas lookup
typedef struct {
    u16 x;
    u16 y;
    u8 w;
    u8 h;
    u8 page;
} FontGlyph;

typedef struct {
    const texture_t* pages;
    int page_count;
    int page_w;
    int page_h;
    int glyph_h;
    u32 range_start;
    u32 range_end;
    u32 default_cp;
    const FontGlyph* glyphs; // size = range_end - range_start + 1
} FontAtlas;

const FontAtlas* font_atlas_get(int font_height);
const FontGlyph* font_atlas_lookup(const FontAtlas* atlas, u32 codepoint);
void font_atlas_prepare(void);

#endif
