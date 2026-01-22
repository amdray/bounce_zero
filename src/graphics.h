#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <psptypes.h>
#include "png.h"  // Для texture_t в batch функциях

// PSP VRAM буферы должны иметь ширину кратную степени двойки для оптимизации
// Экран 480px округляется до 512px - стандартное требование PSP SDK
#define VRAM_BUFFER_WIDTH  512
#define VRAM_BUFFER_HEIGHT 272

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ПРОТОКОЛ ИСПОЛЬЗОВАНИЯ ГРАФИЧЕСКОЙ СИСТЕМЫ:
 *
 * 1. Всегда вызывайте graphics_begin_plain() перед рисованием фигур
 * 2. Всегда вызывайте graphics_begin_textured() перед рисованием спрайтов
 * 3. НЕ вызывайте напрямую sceGuEnable/Disable(GU_TEXTURE_2D)
 * 4. graphics_end_frame() автоматически делает flush всех батчей
 *
 * Переключение режимов автоматически сбрасывает накопленные батчи.
 */

/**
 * Инициализация графической подсистемы
 */
void graphics_init(void);

/**
 * Завершение работы графической подсистемы
 */
void graphics_shutdown(void);

/**
 * Начать новый кадр
 */
void graphics_start_frame(void);

/**
 * Завершить текущий кадр и отобразить на экране
 */
void graphics_end_frame(void);

/**
 * Установить scissor на полный экран
 */
void graphics_set_scissor_fullscreen(void);

/**
 * Очистить экран указанным цветом (без изменения scissor)
 *
 * @param color Цвет в формате ABGR (нативный для PSP GU_PSM_8888).
 *
 *        Формат: 0xAABBGGRR где:
 *        - AA = Alpha (прозрачность, FF = непрозрачный)
 *        - BB = Blue  (синий компонент)
 *        - GG = Green (зеленый компонент)
 *        - RR = Red   (красный компонент)
 *
 *        Примеры основных цветов:
 *        - Красный:   0xFF0000FF (A=FF, B=00, G=00, R=FF)
 *        - Зеленый:   0xFF00FF00 (A=FF, B=00, G=FF, R=00)
 *        - Синий:     0xFFFF0000 (A=FF, B=FF, G=00, R=00)
 *        - Белый:     0xFFFFFFFF (A=FF, B=FF, G=FF, R=FF)
 *        - Черный:    0xFF000000 (A=FF, B=00, G=00, R=00)
 */
void graphics_clear(u32 color);


/**
 * Нарисовать прямоугольник
 * @param x X координата левого верхнего угла
 * @param y Y координата левого верхнего угла
 * @param w Ширина прямоугольника
 * @param h Высота прямоугольника
 * @param color Цвет в формате ABGR (тот же что и в graphics_clear())
 */
void graphics_draw_rect(float x, float y, float w, float h, u32 color);

/**
 * Нарисовать текст с выбором шрифта
 * @param x X координата левого верхнего угла текста
 * @param y Y координата левого верхнего угла текста
 * @param text Строка для отображения
 * @param color Цвет текста в формате ABGR (тот же что и в graphics_clear())
 * @param font_height Высота шрифта: 9 (font9), 12 (font12), или 23 (font23)
 */
void graphics_draw_text(float x, float y, const char* text, u32 color, int font_height);

/**
 * Измерить ширину текста для выбранного шрифта
 * @param text Строка для измерения
 * @param font_height Высота шрифта: 9 (font9), 12 (font12), или 23 (font23)
 * @return Ширина текста в пикселях
 */
int graphics_measure_text(const char* text, int font_height);

/**
 * Нарисовать число используя font24 (только цифры 0-9)
 * @param x X координата
 * @param y Y координата
 * @param number Число для отрисовки
 * @param color Цвет в формате ABGR8888
 */
void graphics_draw_number(float x, float y, int number, u32 color);

/**
 * Измерить ширину числа для font24
 * @param number Число для измерения
 * @return Ширина числа в пикселях
 */
int graphics_measure_number(int number);



/**
 * Декодировать UTF-8 символ в Unicode codepoint
 * @param str UTF-8 строка
 * @param bytes_read Количество прочитанных байт
 * @return Unicode codepoint, для ошибки возвращает пробел U+0020
 */
int utf8_decode_to_codepoint(const char* str, int* bytes_read);

/**
 * Единое управление состоянием текстур
 * ВАЖНО: Вне graphics.* запрещены прямые вызовы sceGuEnable/Disable(GU_TEXTURE_2D)!
 */
void graphics_set_texturing(int enabled);  // 0=off, 1=on
void graphics_begin_plain(void);           // гарантирует выключенные текстуры (HUD фон/текст)
void graphics_begin_textured(void);        // гарантирует включенные текстуры + сброс цвета (ОБЯЗАТЕЛЬНО перед PNG!)
int graphics_get_texturing_state(void);    // получить текущее состояние (0=plain, 1=textured)

/**
 * Sprite batching система для оптимизации рендера (по образцу pspsdk/samples/gu/sprite)
 * Объединяет множественные draw вызовы в пачки для минимизации texture bind'ов
 */
void graphics_bind_texture(texture_t* tex);         // Привязать текстуру для batch'а
void graphics_batch_sprite(float u1, float v1, float u2, float v2, 
                          float x, float y, float w, float h); // Добавить спрайт в batch
void graphics_batch_sprite_colored(float u1, float v1, float u2, float v2,
                          float x, float y, float w, float h, u32 color); // Для текста/модуляции (GU_TFX_MODULATE)
void graphics_flush_batch(void);                    // Принудительно отрисовать накопленные спрайты



#ifdef __cplusplus
}
#endif

#endif
