#ifndef INPUT_H
#define INPUT_H

#include <pspctrl.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Инициализация подсистемы ввода
 * Настраивает контроллер и обнуляет состояния
 */
void input_init(void);

/**
 * Обновить состояние контроллера
 * Должна вызываться каждый кадр для корректной работы input_pressed()/input_held()
 */
void input_update(void);

/**
 * Проверить нажатие кнопки (одноразовое срабатывание)
 * @param button Битовая маска кнопки (см. PSP_CTRL_* константы в <pspctrl.h>)
 * @return true если кнопка только что была нажата, иначе false
 */
bool input_pressed(unsigned int button);

/**
 * Проверить удержание кнопки (непрерывное срабатывание)
 * @param button Битовая маска кнопки (см. PSP_CTRL_* константы в <pspctrl.h>)
 * @return true если кнопка в данный момент удерживается, иначе false
 */
bool input_held(unsigned int button);

/**
 * Проверить отпускание кнопки (одноразовое срабатывание при отпускании)
 * @param button Битовая маска кнопки (см. PSP_CTRL_* константы в <pspctrl.h>)
 * @return true если кнопка только что была отпущена, иначе false
 */
bool input_released(unsigned int button);

/**
 * Получить и сбросить событие нажатия (edge)
 * Используется для фиксированного тика, чтобы не дублировать нажатия
 */
bool input_consume_pressed(unsigned int button);

/**
 * Получить и сбросить событие отпускания (edge)
 * Используется для фиксированного тика, чтобы не дублировать отпускания
 */
bool input_consume_released(unsigned int button);

/**
 * Сбросить накопленные edge-события
 * Полезно при смене состояния игры
 */
void input_reset_edges(void);

/**
 * Заблокировать уже зажатые кнопки до их отпускания
 * Нужен, чтобы после респауна удержание не считалось новым вводом
 */
void input_lock_held(void);

#ifdef __cplusplus
}
#endif

#endif
