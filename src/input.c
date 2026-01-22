#include "input.h"
#include <string.h>
#include <stdbool.h>

static unsigned int s_buttons = 0;
static unsigned int s_prev_buttons = 0;
static unsigned int s_pressed_frame = 0;
static unsigned int s_released_frame = 0;
static unsigned int s_pressed_accum = 0;
static unsigned int s_released_accum = 0;
static unsigned int s_lock_mask = 0;

void input_init(void) {
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL); // Только цифровые кнопки - Bounce не использует аналоговый стик по дизайну
}

void input_update(void) {
    // Текущее состояние кнопок для input_held
    SceCtrlData pad;
    if (sceCtrlReadBufferPositive(&pad, 1) <= 0) {
        return;
    }
    s_buttons = pad.Buttons;

    unsigned int pressed = s_buttons & ~s_prev_buttons;
    unsigned int released = ~s_buttons & s_prev_buttons;
    s_pressed_frame = pressed;
    s_released_frame = released;
    s_pressed_accum |= pressed;
    s_released_accum |= released;
    s_prev_buttons = s_buttons;

    // Разблокируем удержание, когда кнопку отпустили
    s_lock_mask &= ~released;
}

bool input_pressed(unsigned int button) {
    return (s_pressed_frame & button & ~s_lock_mask);
}

bool input_held(unsigned int button) {
    return (s_buttons & button & ~s_lock_mask);
}

bool input_released(unsigned int button) {
    return (s_released_frame & button);
}

bool input_consume_pressed(unsigned int button) {
    unsigned int masked = s_pressed_accum & ~s_lock_mask;
    if (masked & button) {
        s_pressed_accum &= ~button;
        return true;
    }
    return false;
}

bool input_consume_released(unsigned int button) {
    if (s_released_accum & button) {
        s_released_accum &= ~button;
        return true;
    }
    return false;
}

void input_reset_edges(void) {
    s_pressed_accum = 0;
    s_released_accum = 0;
    s_pressed_frame = 0;
    s_released_frame = 0;
    s_prev_buttons = s_buttons;
}

void input_lock_held(void) {
    s_lock_mask |= s_buttons;
}
