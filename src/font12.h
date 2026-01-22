#ifndef FONT12_H
#define FONT12_H

#include <psptypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FONT12_HEIGHT 12
#define FONT12_COUNT 194  // Таблица: 0-127 ASCII + 128-160 А-Я + 161-193 а-я
#define FONT12_SPACING 0  // Межсимвольный отступ (0 = промежутки вшиты в ширину символов)

// Индексы для прямого доступа к символам
#define FONT12_ASCII_START 0
#define FONT12_CYRILLIC_UPPER_START 128  // А-Я
#define FONT12_CYRILLIC_LOWER_START 161  // а-я

typedef struct {
    u16 row[FONT12_HEIGHT];  // 12 строк пиксельных данных (до 16 пикселей ширины)
    u8 width;         // Ширина символа в пикселях
} Glyph12;

/**
 * Получить указатель на таблицу шрифта
 * @return Указатель на массив всех глифов
 */
const Glyph12* font12_table(void);

/**
 * Получить глиф по индексу в таблице (прямой доступ O(1))
 * @param index Индекс в таблице 0-193
 * @return Указатель на глиф. Для неверного индекса возвращает пробел
 */
const Glyph12* font12_get_glyph_by_index(int index);



#ifdef __cplusplus
}
#endif

#endif
