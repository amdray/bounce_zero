#ifndef FONT9_H
#define FONT9_H

#include <psptypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FONT9_HEIGHT 9
#define FONT9_COUNT 194  // Новая таблица: 0-127 ASCII + 128-160 А-Я + 161-193 а-я
#define FONT9_SPACING 0  // Межсимвольный отступ (0 = промежутки вшиты в ширину символов)

// Индексы для прямого доступа к символам
#define FONT9_ASCII_START 0
#define FONT9_CYRILLIC_UPPER_START 128  // А-Я
#define FONT9_CYRILLIC_LOWER_START 161  // а-я

typedef struct {
    u16 row[FONT9_HEIGHT];  // 9 строк пиксельных данных (до 16 пикселей ширины)
    u8 width;         // Ширина символа в пикселях
} Glyph9;

/**
 * Получить указатель на таблицу шрифта
 * @return Указатель на массив всех глифов
 */
const Glyph9* font9_table(void);

/**
 * Получить глиф по индексу в таблице (прямой доступ O(1))
 * @param index Индекс в таблице 0-193
 * @return Указатель на глиф. Для неверного индекса возвращает пробел
 */
const Glyph9* font9_get_glyph_by_index(int index);



#ifdef __cplusplus
}
#endif

#endif
