#include "splash.h"
#include "game.h"
#include "graphics.h"
#include "input.h"
#include "level.h"
#include "local_extra.h"
#include "png.h"
#include "types.h"
#include "graphics.h"

// Утилита для центрирования текстур на экране (splash screens)
static void draw_centered_splash(texture_t* tex) {
    if (!tex) return;
    graphics_begin_textured();

    // Используем реальный размер изображения, а не размер текстуры GPU
    int real_width = tex->actual_width;
    int real_height = tex->actual_height;

    sprite_rect_t sprite = png_create_sprite_rect(tex, 0, 0, real_width, real_height);
    int x = (SCREEN_WIDTH - real_width) / 2;
    int y = (SCREEN_HEIGHT - real_height) / 2;
    png_draw_sprite(tex, &sprite, x, y, real_width, real_height);
    graphics_flush_batch();
}

void splash_update_nokia(void) {
    // Увеличиваем таймер
    g_game.splash_timer++;

    // Переход к Bounce splash через 90 кадров (3 секунды при 30fps)
    if (g_game.splash_timer >= 90 || input_pressed(PSP_CTRL_CROSS) || input_pressed(PSP_CTRL_START)) {
        g_game.state = STATE_SPLASH;
        g_game.splash_timer = 0;
    }
}

void splash_update_bounce(void) {
    // Переход к меню только по START (убрали таймер и X)
    if (input_pressed(PSP_CTRL_START)) {
        g_game.state = STATE_MENU;
        g_game.splash_timer = 0;
    }
}

void splash_render_nokia(void) {
    // Черный фон для Nokia Games splash
    graphics_clear(COLOR_TEXT_NORMAL);

    draw_centered_splash(g_game.nokia_splash_texture);
}

void splash_render_bounce(void) {
    // Тот же фон что у About экрана
    graphics_clear(ABOUT_BACKGROUND_COLOUR);

    draw_centered_splash(g_game.bounce_splash_texture);

    graphics_begin_plain();

    // Надпись "Press START" под PNG
    int text_y = 136 + 70;
    const char* press_start_text = local_text_press_start();

    // Центрируем текст по горизонтали
    int text_width = graphics_measure_text(press_start_text, 12);
    int text_x = (SCREEN_WIDTH - text_width) / 2;

    graphics_draw_text((float)text_x, (float)text_y, press_start_text, COLOR_TEXT_NORMAL, 12);
}
