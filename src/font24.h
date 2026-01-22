#ifndef FONT24_H
#define FONT24_H

#include <pspge.h>
#include <psptypes.h>

#define FONT24_HEIGHT 24
#define FONT24_SPACING 0

typedef struct {
    u16 row[FONT24_HEIGHT];  // 24 строки пиксельных данных (до 16 пикселей ширины)
    u8 width;
} Glyph24;

// Получить глиф для цифры (0-9)
const Glyph24* font24_get_digit(int digit);

#endif // FONT24_H
