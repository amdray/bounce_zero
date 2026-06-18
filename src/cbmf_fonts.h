#ifndef CBMF_FONTS_H
#define CBMF_FONTS_H

#include "cbmf.h"
#include "cbmf_psp.h"

int  cbmf_fonts_init(void);
void cbmf_fonts_shutdown(void);

CbmfPspRenderer *cbmf_fonts_get_renderer(int font_height);
const CbmfFont  *cbmf_fonts_get_font(int font_height);

#endif
