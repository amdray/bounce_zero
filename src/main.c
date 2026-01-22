#include <pspkernel.h>
#include <pspdisplay.h>
#include <stdio.h>
#include <string.h>
#include "graphics.h"
#include "input.h"
#include "game.h"
#include "sound.h"
#include "types.h"

PSP_MODULE_INFO("2D Platformer", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);

// Макрос для подавления предупреждений о неиспользуемых параметрах
#define UNUSED(x) ((void)(x))

// Фиксированный тик физики в миллисекундах (как в Java: 30 мс ≈ 33.3 Гц)
#define PHYSICS_DT_MS 30


// Параметры потока колбэков (из образца PSPSDK)
#define CALLBACK_PRIO 0x11
#define CALLBACK_STACK 0xFA0

/**
 * Open file using current working directory.
 */
FILE* util_open_file(const char* path, const char* mode) {
    if (!path || !mode) return NULL;
    return fopen(path, mode);
}

/**
 * Callback для выхода из приложения при нажатии HOME
 */
int main_exit_callback(int arg1, int arg2, void *common) {
    UNUSED(arg1); UNUSED(arg2); UNUSED(common);
    g_game.state = STATE_EXIT;  // Элегантное завершение через игровой цикл
    return 0;
}

/**
 * Поток для обработки системных callback'ов PSP
 */
int main_callback_thread(SceSize args, void *argp) {
    UNUSED(args); UNUSED(argp);
    int cbid = sceKernelCreateCallback("Exit Callback", main_exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

int main_setup_callbacks(void) {
    int thid = sceKernelCreateThread("callback_thread", main_callback_thread, CALLBACK_PRIO, CALLBACK_STACK, 0, 0);
    if(thid >= 0)
        sceKernelStartThread(thid, 0, 0);
    return thid;
}

int main(void) {
    main_setup_callbacks();
    
    graphics_init();
    input_init();
    if (sound_init() < 0) {
        graphics_shutdown();
        sceKernelExitGame();
        return 0;
    }
    save_init();      // Загрузить сохранённые рекорды
    game_init();
    
    // Тайминг для фиксированного тика физики (30 мс как в Java TileCanvas.GameTimer)
    unsigned long long prev_time_ms = sceKernelGetSystemTimeWide() / 1000ULL;
    int physics_time_acc_ms = 0;
    GameState prev_state = g_game.state;
    
    while(g_game.state != STATE_EXIT) {
        input_update();

        const game_state_handler_t* handler = game_get_state_handler(g_game.state);
        if (!handler) break;
        if (g_game.state != prev_state) {
            if (g_game.state == STATE_MENU && prev_state != STATE_MENU) {
                save_flush();
            }
            input_reset_edges();
            prev_state = g_game.state;
        }

        // === ОБНОВЛЕНИЕ МЕНЮ И ИНТЕРФЕЙСА (каждый кадр) ===
        if (handler->tick_mode == GAME_TICK_VARIABLE) {
            game_state_update();  // Все UI состояния обновляются на полной частоте 60 FPS для отзывчивости
        }

        // === ОБНОВЛЕНИЕ ИГРОВОЙ ФИЗИКИ (фиксированный тик 30 мс) ===
        else if (handler->tick_mode == GAME_TICK_FIXED) {
            // ВАЖНО: Кнопки меню обрабатываются на полной частоте рендера,
            // физика вызывается отдельным фиксированным тиком (30 мс),
            // чтобы избежать пропусков нажатий
            if(input_pressed(PSP_CTRL_START)) {
                // Сохраняем состояние игры для Continue
                g_game.saved_game_state = SAVED_GAME_IN_PROGRESS;
                g_game.state = STATE_MENU;
            }

            // Аккумулятор времени для вызова game_update() ровно раз в 30 мс
            unsigned long long now_ms = sceKernelGetSystemTimeWide() / 1000ULL;
            int delta_ms = (int)(now_ms - prev_time_ms);
            prev_time_ms = now_ms;

            // Предотвращаем огромные скачки (например, при паузе)
            if (delta_ms > 200) delta_ms = 200;

            physics_time_acc_ms += delta_ms;

            while (physics_time_acc_ms >= PHYSICS_DT_MS) {
                game_state_update();

                physics_time_acc_ms -= PHYSICS_DT_MS;

                handler = game_get_state_handler(g_game.state);
                if (!handler || handler->tick_mode != GAME_TICK_FIXED) {
                    break;
                }
            }
        }
        
        // Рендеринг всегда на полной частоте для плавности
        graphics_start_frame();
        game_state_render();
        graphics_end_frame();
        
    }
    
    game_shutdown();
    save_shutdown();   // Сохранить рекорды перед выходом
    sound_shutdown();
    graphics_shutdown();
    
    sceKernelExitGame();
    return 0;
}
