#ifndef FONT16_H
#define FONT16_H

#include <psptypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FONT16_HEIGHT 16
#define FONT16_COUNT 194  // Новая таблица: 0-127 ASCII + 128-160 А-Я + 161-193 а-я
#define FONT16_SPACING 1  // Межсимвольный отступ при рендеринге

// Индексы для прямого доступа к символам
#define FONT16_ASCII_START 0
#define FONT16_CYRILLIC_UPPER_START 128  // А-Я
#define FONT16_CYRILLIC_LOWER_START 161  // а-я

typedef struct {
    u8 row[FONT16_HEIGHT];  // 16 строк пиксельных данных
    u8 width;         // Ширина символа в пикселях
} Glyph16;

/**
 * Получить указатель на таблицу шрифта
 * @return Указатель на массив всех глифов
 */
const Glyph16* font16_table(void);

/**
 * Получить глиф по индексу в таблице (прямой доступ O(1))
 * @param index Индекс в таблице 0-193
 * @return Указатель на глиф. Для неверного индекса возвращает пробел
 */
const Glyph16* font16_get_glyph_by_index(int index);



#ifdef __cplusplus
}
#endif

#endif