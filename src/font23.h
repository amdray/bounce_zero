#ifndef FONT23_H
#define FONT23_H

#include <psptypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FONT23_HEIGHT 23
#define FONT23_COUNT 194  // Новая таблица: 0-127 ASCII + 128-160 А-Я + 161-193 а-я
#define FONT23_SPACING 0  // Межсимвольный отступ (0 = промежутки вшиты в ширину символов)

// Индексы для прямого доступа к символам
#define FONT23_ASCII_START 0
#define FONT23_CYRILLIC_UPPER_START 128  // А-Я
#define FONT23_CYRILLIC_LOWER_START 161  // а-я

typedef struct {
    u32 row[FONT23_HEIGHT];  // 23 строки пиксельных данных (до 32 пикселей ширины)
    u8 width;          // Ширина символа в пикселях
} Glyph23;

/**
 * Получить указатель на таблицу шрифта
 * @return Указатель на массив всех глифов
 */
const Glyph23* font23_table(void);

/**
 * Получить глиф по индексу в таблице (прямой доступ O(1))
 * @param index Индекс в таблице 0-193
 * @return Указатель на глиф. Для неверного индекса возвращает пробел
 */
const Glyph23* font23_get_glyph_by_index(int index);



#ifdef __cplusplus
}
#endif

#endif
