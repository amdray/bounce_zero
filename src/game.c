#include "game.h"
#include "graphics.h"
#include "input.h"
#include "types.h"
#include "level.h"  // Содержит #include "png.h" и extern объявления
#include "tile_table.h" // Для TILE_SIZE
#include "menu.h"
#include "sound.h"  // OTT audio support
#include "local.h"  // Для локализации
#include "splash.h"
#include <pspctrl.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

// Forward declarations

Game g_game;

// Анимация двери (как в Java original)
static int g_exit_animation_offset = 0;  // 0..24 пикселей смещения вверх
static bool g_exit_is_opening = false;      // флаг активной анимации

// extern texture_t* g_tileset;
// extern int g_tiles_per_row;

// Константы форматирования перенесены в types.h

// Форматирование счёта с ведущими нулями (как в оригинале)
static void format_score_string(int score, char* buffer) {
    // Используем %0*d для автоматического нулевого заполнения
    snprintf(buffer, SCORE_DIGITS+1, "%0*d", SCORE_DIGITS, score);
}

// Отрисовка полоски бонуса (как в оригинальном Java BounceCanvas)
static void draw_bonus_bar(int x, int y, int bonus_value) {
    // Размеры: рамка 62x10, полоска 60x8
    const int frame_width = 62;
    const int frame_height = 10;
    const int bar_width = 60;
    const int bar_height = 8;
    
    // Рисуем белую рамку (контур толщиной 1 пиксель)
    graphics_draw_rect((float)x, (float)y, (float)frame_width, 1.0f, COLOR_BONUS_FRAME);                    // Верх
    graphics_draw_rect((float)x, (float)(y + frame_height - 1), (float)frame_width, 1.0f, COLOR_BONUS_FRAME); // Низ
    graphics_draw_rect((float)x, (float)y, 1.0f, (float)frame_height, COLOR_BONUS_FRAME);                   // Лево
    graphics_draw_rect((float)(x + frame_width - 1), (float)y, 1.0f, (float)frame_height, COLOR_BONUS_FRAME);  // Право
    
    if (bonus_value > 0) {
        // Вычисляем ширину полоски (уменьшается слева направо)
        int current_bar_width = (bar_width * bonus_value) / 300;
        
        if (current_bar_width > 0) {
            // Оранжевая полоска с отступом 1px со всех сторон (внутри рамки)
            graphics_draw_rect((float)(x + 1), (float)(y + 1), (float)current_bar_width, (float)bar_height, COLOR_BONUS_BAR);
        }
    }
}

// Camera - система отслеживания игрока с мертвой зоной
#define CAMERA_UNINITIALIZED -999
#define CAMERA_DEADZONE_RATIO 0.30f   // 30% от игровой области - зона без движения камеры
// Статическая переменная для вертикальной камеры с мертвой зоной
static int s_currentCameraY = CAMERA_UNINITIALIZED;

// HUD размеры
#define HUD_HEIGHT 17               // Высота HUD: 2+12+2px синяя полоса + 1px разделитель

// Проверка является ли уровень маленьким (ниже игровой области по высоте)
// Такие уровни центрируются по вертикали без мертвой зоны камеры
static inline bool is_level_small(void) {
    int gameAreaHeight = SCREEN_HEIGHT - HUD_HEIGHT;
    return (g_level.height * TILE_SIZE) < gameAreaHeight;
}

// Получить смещение камеры для вертикального центрирования маленького уровня
// Возвращает отрицательное значение для центрирования уровня в игровой области
static inline int get_center_offset(void) {
    int levelPixelHeight = g_level.height * TILE_SIZE;
    int gameAreaHeight = SCREEN_HEIGHT - HUD_HEIGHT;
    return -(gameAreaHeight - levelPixelHeight) / 2;
}

void game_reset_camera(void) {
    if (is_level_small()) {
        s_currentCameraY = get_center_offset();
    } else {
        s_currentCameraY = CAMERA_UNINITIALIZED; // Будет инициализирована позицией игрока
    }
}

void game_init(void) {
    g_game.state = STATE_SPLASH_NOKIA;
    g_game.menu_selection = 0;
    g_game.selected_level = 1;
    
    // Инициализация saved game state
    g_game.saved_game_state = SAVED_GAME_NONE;

    // Сброс камеры для предотвращения устаревших значений
    game_reset_camera();

    // Отладочные флаги
    g_game.invincible_cheat = false;      // Читерское бессмертие выключено по умолчанию
    
    // Инициализация системы колец (как в Java BounceCanvas.resetGame)
    g_game.numRings = 0;
    g_game.score = 0;
    g_game.numLives = 3;        // Начинаем с 3 жизней как в оригинале
    
    // Сброс анимации двери
    game_exit_reset();
    
    // Инициализация splash экранов
    g_game.splash_timer = 0;
    g_game.nokia_splash_texture = png_load_texture_vram(SPLASH_NAME_NOKIA);
    g_game.bounce_splash_texture = png_load_texture_vram(SPLASH_NAME_BOUNCE);
    
    // Инициализация меню
    menu_init();
    
    // Загружаем уровень 1
    if (level_load_by_number(1)) {
        game_reset_camera();
    }
    
    // Инициализация игрока из данных уровня
    player_init(&g_game.player, g_level.startPosX, g_level.startPosY,
                g_level.ballSize == BALL_SIZE_SMALL ? SMALL_SIZE_STATE : LARGE_SIZE_STATE);

    // Устанавливаем респавн в стартовую позицию (как в Java: mBall.setRespawn())
    // Используем исходные тайловые координаты (как в Java: mStartPosX/Y)
    level_set_respawn(g_level.startTileX, g_level.startTileY);
}

void game_start_level(int level_number, game_start_mode_t mode) {
    g_game.state = STATE_GAME;
    g_game.selected_level = level_number;

    if (level_load_by_number(level_number)) {
        game_reset_camera();
    }

    // Устанавливаем респавн в стартовую позицию (как в game_init)
    level_set_respawn(g_level.startTileX, g_level.startTileY);

    if (mode == GAME_START_FRESH || mode == GAME_START_SELECTED) {
        // Сброс счётчиков при старте уровня (как в Java BounceCanvas.startLevel)
        g_game.numRings = 0;
        g_game.score = 0;
        g_game.numLives = 3;
    } else if (mode == GAME_START_NEXT) {
        // При переходе на следующий уровень сохраняем счет/жизни
        g_game.numRings = 0;
    }

    // Дверь всегда должна начинать уровень закрытой
    game_exit_reset();

    if (mode == GAME_START_FRESH) {
        g_game.saved_game_state = SAVED_GAME_IN_PROGRESS;
        g_game.new_best_score = false;
    }

    player_init(&g_game.player, g_level.startPosX, g_level.startPosY,
                g_level.ballSize == BALL_SIZE_SMALL ? SMALL_SIZE_STATE : LARGE_SIZE_STATE);
}

static void update_menu_common(void) {
    menu_type_t menu_type = menu_get_type_from_game_state(g_game.state);
    menu_update_by_type(menu_type);
}

static void update_game(void) {
    // Обновление направления из инпута
    Player* player = &g_game.player;

    // Java-совместимая архитектура: ввод управляет флагами, физика их только читает

    // Движение ВЛЕВО — учитываем и событие press/release, и удержание
    if (input_consume_pressed(PSP_CTRL_LEFT) || input_held(PSP_CTRL_LEFT)) {
        set_direction(player, MOVE_LEFT);
    }
    if (input_consume_released(PSP_CTRL_LEFT) || !input_held(PSP_CTRL_LEFT)) {
        release_direction(player, MOVE_LEFT);
    }

    // Движение ВПРАВО — учитываем и событие press/release, и удержание
    if (input_consume_pressed(PSP_CTRL_RIGHT) || input_held(PSP_CTRL_RIGHT)) {
        set_direction(player, MOVE_RIGHT);
    }
    if (input_consume_released(PSP_CTRL_RIGHT) || !input_held(PSP_CTRL_RIGHT)) {
        release_direction(player, MOVE_RIGHT);
    }

    // ПРЫЖОК: 100% Java-совместимая логика
    // keyPressed -> set_direction, keyReleased -> release_direction
    if (input_consume_pressed(PSP_CTRL_CROSS)) {
        set_direction(player, MOVE_UP);
    }
    // Отпускание прыжка через буфер (красивая архитектура!)
    if (input_consume_released(PSP_CTRL_CROSS) || !input_held(PSP_CTRL_CROSS)) {
        release_direction(player, MOVE_UP);
    }

    // Обновление физики игрока (частота 30 FPS управляется из main.c)
    player_update(player);

    // Обработка смерти игрока (как в Java BounceCanvas.java:569-580)
    if (player->ballState == BALL_STATE_DEAD) {
        // ВАЖНО: нестандартная логика жизней (как в оригинале Java Bounce):
        // numLives: 3→2→1→0→(-1). Game Over при < 0, т.к. при 0 еще остается последняя попытка
        // Это означает: 3 жизни = 4 попытки игры (3 обычные + 1 последняя при numLives=0)
        if (g_game.numLives < 0) {
            // Game Over - обновить рекорды и показать Game Over экран (как в оригинале BounceCanvas:440)
            save_update_records(g_game.selected_level, g_game.score);
            g_game.saved_game_state = SAVED_GAME_NONE;
            g_game.state = STATE_GAME_OVER;
        } else {
            // Респаун - сохраняем ТЕКУЩИЙ размер мяча (основная цель этой задачи!)
            BallSizeState currentSize = player->sizeState;  // СОХРАНЯЕМ размер

            // Получаем координаты респауна (чекпоинта)
            int respawnX, respawnY;
            level_get_respawn(&respawnX, &respawnY);

            // Респаун в точке чекпоинта с сохранённым размером (как в оригинале)
            int respawnHalf = (currentSize == SMALL_SIZE_STATE) ? HALF_NORMAL_SIZE : HALF_ENLARGED_SIZE;
            player_init(player, respawnX * TILE_SIZE + respawnHalf, respawnY * TILE_SIZE + respawnHalf, currentSize);

            // Сброс камеры к игроку
            game_reset_camera();

            // После респауна удержание кнопок не должно считаться новым вводом
            input_reset_edges();
            input_lock_held();
        }
    }

    // Обновление движущихся объектов
    level_update_moving_objects();

    // Обновление анимации двери (как в Java openExit())
    if (g_exit_is_opening) {
        g_exit_animation_offset += 4;  // увеличиваем на 4 каждый кадр
        if (g_exit_animation_offset >= 24) {
            g_exit_animation_offset = 24;  // максимум 24 пикселя
            g_exit_is_opening = false;        // анимация завершена
        }
    }

    // L+R = переключить читерское бессмертие (как mInvincible в Java)
    if (input_consume_pressed(PSP_CTRL_LTRIGGER) && input_held(PSP_CTRL_RTRIGGER)) {
        g_game.invincible_cheat = !g_game.invincible_cheat;
        // Звук активации чита (как в оригинале mSoundHoop.play(1))
        sound_play_hoop();
    }

    // Пауза теперь обрабатывается в main.c на полной частоте
}

static void update_noop(void) {}

static void render_menu_common(void) {
    graphics_clear(BACKGROUND_COLOUR);
    menu_type_t menu_type = menu_get_type_from_game_state(g_game.state);
    menu_render_by_type(menu_type);
}

static void render_game(void) {
    graphics_clear(BACKGROUND_COLOUR);

    Player* player = &g_game.player;

    // Горизонтальная камера - следует за игроком (как раньше)
    int cameraX = player->xPos - SCREEN_WIDTH / 2;

    // Вертикальная камера с мертвой зоной
    if (s_currentCameraY == CAMERA_UNINITIALIZED) {
        // Первая инициализация - только для больших уровней
        int gameAreaHeight = SCREEN_HEIGHT - HUD_HEIGHT;
        s_currentCameraY = player->yPos - gameAreaHeight / 2;
    }

    // Размеры мертвой зоны (только для игровой области)
    int gameAreaHeight = SCREEN_HEIGHT - HUD_HEIGHT;
    int deadZoneTop = (int)(gameAreaHeight * CAMERA_DEADZONE_RATIO);
    int deadZoneBottom = gameAreaHeight - deadZoneTop;

    // Мертвая зона работает только для больших уровней
    if (!is_level_small()) {
        int tempPlayerScreenY = player->yPos - s_currentCameraY;

        if (tempPlayerScreenY < deadZoneTop) {
            // Игрок слишком высоко - двигаем камеру вверх
            s_currentCameraY = player->yPos - deadZoneTop;
        } else if (tempPlayerScreenY > deadZoneBottom) {
            // Игрок слишком низко - двигаем камеру вниз
            s_currentCameraY = player->yPos - deadZoneBottom;
        }
        // Иначе камера остается на месте (мертвая зона)
    }

    int cameraY = s_currentCameraY;

    // Ограничиваем камеру границами уровня (учитываем игровую область)
    int maxCameraX = g_level.width * TILE_SIZE - SCREEN_WIDTH;
    int maxCameraY = g_level.height * TILE_SIZE - gameAreaHeight;

    if (cameraX < 0) cameraX = 0;
    if (cameraX > maxCameraX && maxCameraX > 0) cameraX = maxCameraX;

    if (is_level_small()) {
        // Для маленьких уровней фиксируем камеру по центру
        cameraY = get_center_offset();
    } else {
        // Обычное ограничение для больших уровней
        if (cameraY < 0) cameraY = 0;
        if (cameraY > maxCameraY && maxCameraY > 0) cameraY = maxCameraY;
    }

    // Рендерим уровень (исключая область HUD)
    level_set_ring_fg_defer(1);
    level_render_visible_area(cameraX, cameraY, SCREEN_WIDTH, gameAreaHeight);

    // Отладочная визуализация коллизий колец

    // Гарантируем текстуры для спрайтов мяча
    graphics_begin_textured();

    // Игрок - позиция относительно камеры
    int playerScreenX = player->xPos - cameraX - player->mHalfBallSize;
    int playerScreenY = player->yPos - cameraY - player->mHalfBallSize;

    // ИСПРАВЛЕНО: Рисуем спрайт шара вместо цветного квадрата
    texture_t* tileset = level_get_tileset();
    if (tileset && level_get_tiles_per_row() > 0) {
        int ballSpriteX, ballSpriteY;

        if (player->ballState == BALL_STATE_POPPED) {
            // Лопнувший шар - tileImages[48] = extractImage(image, 0, 1)
            // Атлас позиция (0,1) = индекс 4
            ballSpriteX = 0 * TILE_SIZE;
            ballSpriteY = 1 * TILE_SIZE;
        } else if (player->sizeState == LARGE_SIZE_STATE) {
            // Большой шар - tileImages[49] = createLargeBallImage(extractImage(image, 3, 0))
            // Атлас позиция (3,0) = индекс 3, но это составной 2x2, используем базовый кусок
            ballSpriteX = 3 * TILE_SIZE;
            ballSpriteY = 0 * TILE_SIZE;
        } else {
            // Маленький шар - tileImages[47] = extractImage(image, 2, 0)
            // Атлас позиция (2,0) = индекс 2
            ballSpriteX = 2 * TILE_SIZE;
            ballSpriteY = 0 * TILE_SIZE;
        }

        sprite_rect_t ballSprite = png_create_sprite_rect(tileset, ballSpriteX, ballSpriteY, TILE_SIZE, TILE_SIZE);

        // Лопнувший мяч всегда рендерится как один спрайт 12x12
        if (player->ballState == BALL_STATE_POPPED) {
            int poppedX = player->xPos - cameraX - HALF_POPPED_SIZE;
            int poppedY = player->yPos - cameraY - HALF_POPPED_SIZE;
            png_draw_sprite(tileset, &ballSprite, poppedX, poppedY, POPPED_SIZE, POPPED_SIZE);
        }
        // Для большого живого мяча рендерим 2x2
        else if (player->sizeState == LARGE_SIZE_STATE) {
            // Рендерим 4 куска для большого мяча (как в Java createLargeBallImage)
            for (int dy = 0; dy < 2; dy++) {
                for (int dx = 0; dx < 2; dx++) {
                    png_transform_t xf = PNG_TRANSFORM_IDENTITY;
                    if (dx == 1) xf = PNG_TRANSFORM_FLIP_X;
                    if (dy == 1 && dx == 0) xf = PNG_TRANSFORM_FLIP_Y;
                    if (dy == 1 && dx == 1) xf = PNG_TRANSFORM_ROT_180;

                    int x = playerScreenX + HALF_ENLARGED_SIZE - TILE_SIZE + dx * TILE_SIZE;
                    int y = playerScreenY + HALF_ENLARGED_SIZE - TILE_SIZE + dy * TILE_SIZE;

                    if (xf == PNG_TRANSFORM_IDENTITY) {
                        png_draw_sprite(tileset, &ballSprite, x, y, TILE_SIZE, TILE_SIZE);
                    } else {
                        png_draw_sprite_transform(tileset, &ballSprite, x, y, TILE_SIZE, TILE_SIZE, xf);
                    }
                }
            }
        } else {
            // Обычный рендер для маленького мяча
            png_draw_sprite(tileset, &ballSprite, playerScreenX, playerScreenY, player->ballSize, player->ballSize);
        }
    }

    // Принудительно сбрасываем batch мяча перед отрисовкой foreground
    graphics_flush_batch();

    // Инструкции и отладка (поверх всего)
    level_flush_ring_foreground();
    level_set_ring_fg_defer(0);

    // Debug hoop collision visualization removed

    // Отладочный оверлей блокирующих тайлов (поверх уровня и игрока)
    graphics_begin_plain();

    // HUD фон/текст (без текстур)
    graphics_begin_plain();

    // HUD: белый разделитель + синяя полоса
    const int hudStartY = SCREEN_HEIGHT - HUD_HEIGHT;
    int separator_y = hudStartY;
    int hud_blue_y = separator_y + 1;

    // Белый разделитель (1 пиксель)
    graphics_draw_rect(0.0f, (float)separator_y, (float)SCREEN_WIDTH, 1.0f, COLOR_WHITE_ABGR);

    // Синяя полоса (16 пикселей: 2+12+2)
    graphics_draw_rect(0.0f, (float)hud_blue_y, (float)SCREEN_WIDTH, 16.0f, HUD_COLOUR);

    // Полоска бонуса (БЕЗ текстур - примитивы)
    // Вычисляем максимальный счетчик бонуса (как bonusCntrValue в Java)
    int max_bonus = 0;
    if (g_game.player.speedBonusCntr > max_bonus) max_bonus = g_game.player.speedBonusCntr;
    if (g_game.player.gravBonusCntr > max_bonus) max_bonus = g_game.player.gravBonusCntr;
    if (g_game.player.jumpBonusCntr > max_bonus) max_bonus = g_game.player.jumpBonusCntr;

    // HUD иконки колец (как в оригинале BounceCanvas.java:340-342)
    // Используем tileset который уже получен выше для мяча
    if (tileset) {
        // Переключаемся на текстурированный режим для спрайтов
        graphics_begin_textured();

        // Иконка кольца из позиции (1,4) в атласе - mUIRing = extractImage(image, 1, 4)
        int srcX = 1 * TILE_SIZE;
        int srcY = 4 * TILE_SIZE;
        sprite_rect_t ringSprite = png_create_sprite_rect(tileset, srcX, srcY, TILE_SIZE, TILE_SIZE);

        // Рисуем НЕсобранные кольца (mTotalNumRings - numRings)
        int remainingRings = g_level.totalRings - g_game.numRings;
        for (int i = 0; i < remainingRings; i++) {
            int x = 5 + i * (TILE_SIZE - 1);  // 5 + i * (mUIRing.getWidth() - 1)
            int y = hudStartY + 3;  // В синей области с отступом 2px сверху
            png_draw_sprite(tileset, &ringSprite, x, y, TILE_SIZE, TILE_SIZE);
        }
    }

    // HUD жизни (с текстурами) - как в оригинале BounceCanvas.java:335-338
    if (tileset) {
        graphics_begin_textured();

        int ballSrcX = 2 * TILE_SIZE;
        int ballSrcY = 1 * TILE_SIZE;
        sprite_rect_t lifeSprite = png_create_sprite_rect(tileset, ballSrcX, ballSrcY, TILE_SIZE, TILE_SIZE);

        for (int i = 0; i < g_game.numLives; i++) {
            int x = SCREEN_WIDTH - 5 - (g_game.numLives - i) * (TILE_SIZE - 1);
            int y = hudStartY + 4;
            png_draw_sprite(tileset, &lifeSprite, x, y, TILE_SIZE, TILE_SIZE);
        }
    }

    // HUD текст (без текстур) - счёт как в оригинале
    graphics_begin_plain();

    // Форматирование и отображение счёта (как в BounceCanvas.java:346)
    char score_buffer[SCORE_DIGITS+1];
    format_score_string(g_game.score, score_buffer);

    // Белый текст по центру HUD (как в оригинале y=100, цвет 16777214)
    int score_width = graphics_measure_text(score_buffer, 9);
    int score_x = (SCREEN_WIDTH - score_width) / 2;
    int score_y = hudStartY + 5;  // Поднял на 1 пиксель
    graphics_draw_text((float)score_x, (float)score_y, score_buffer, COLOR_WHITE_ABGR, 9);

    // Полоска бонуса - справа от счета для симметрии
    {
        int text_width = graphics_measure_text(score_buffer, 9);
        int bonus_x = score_x + text_width + 10 + 30; // После счета с отступом 10px, сдвиг на 30px вправо
        int bonus_y = hudStartY + 4;  // На 1 пиксель ниже (было 3.0f)
        draw_bonus_bar(bonus_x, bonus_y, max_bonus);
    }
}

static void render_noop(void) {}

static const game_state_handler_t s_state_handlers[STATE_EXIT + 1] = {
    [STATE_SPLASH_NOKIA] = { splash_update_nokia, splash_render_nokia, GAME_TICK_VARIABLE },
    [STATE_SPLASH] = { splash_update_bounce, splash_render_bounce, GAME_TICK_VARIABLE },
    [STATE_MENU] = { update_menu_common, render_menu_common, GAME_TICK_VARIABLE },
    [STATE_LEVEL_SELECT] = { update_menu_common, render_menu_common, GAME_TICK_VARIABLE },
    [STATE_GAME] = { update_game, render_game, GAME_TICK_FIXED },
    [STATE_HIGH_SCORE] = { update_menu_common, render_menu_common, GAME_TICK_VARIABLE },
    [STATE_INSTRUCTIONS] = { update_menu_common, render_menu_common, GAME_TICK_VARIABLE },
    [STATE_LEVEL_COMPLETE] = { update_menu_common, render_menu_common, GAME_TICK_VARIABLE },
    [STATE_GAME_OVER] = { update_menu_common, render_menu_common, GAME_TICK_VARIABLE },
    [STATE_EXIT] = { update_noop, render_noop, GAME_TICK_VARIABLE }
};

const game_state_handler_t* game_get_state_handler(GameState state) {
    if (state < 0 || state >= (GameState)(sizeof(s_state_handlers) / sizeof(s_state_handlers[0]))) {
        return NULL;
    }
    return &s_state_handlers[state];
}

void game_state_update(void) {
    const game_state_handler_t* handler = game_get_state_handler(g_game.state);
    if (handler && handler->update) {
        handler->update();
    }
}

void game_state_render(void) {
    const game_state_handler_t* handler = game_get_state_handler(g_game.state);
    if (handler && handler->render) {
        handler->render();
    }
}

void game_update(void) {
    game_state_update();
}

void game_render(void) {
    game_state_render();
}


// Добавляем функцию cleanup для game
void game_shutdown(void) {
    // Освобождаем splash текстуры
    if (g_game.nokia_splash_texture) {
        png_free_texture(g_game.nokia_splash_texture);
        g_game.nokia_splash_texture = NULL;
    }
    if (g_game.bounce_splash_texture) {
        png_free_texture(g_game.bounce_splash_texture);
        g_game.bounce_splash_texture = NULL;
    }
    
    menu_cleanup();
    level_cleanup();
}

void game_add_score(int points) {
    g_game.score += points;
}

void game_add_ring(void) {
    game_add_score(RING_POINTS);    // 1. Добавить очки за кольцо
    g_game.numRings++;      // 2. Увеличить счетчик колец
    
    // 3. Анимация двери запускается из game_ring_collected()
}

void game_set_respawn(int x, int y) {
    level_deactivate_old_checkpoint();        // Деактивируем старый чекпоинт (7->8)
    level_set_respawn(x, y);                  // Устанавливаем новые координаты
    level_mark_checkpoint_active(x, y);       // Активируем новый чекпоинт
    sound_play_pickup();                      // Звук активации чекпоинта
}

void game_add_extra_life(void) {
    // Как в оригинале Ball.java:800-810
    game_add_score(LIFE_POINTS);           // +очки за дополнительную жизнь
    
    if (g_game.numLives < 5) {      // максимум 5 жизней
        g_game.numLives++;
    }
    sound_play_pickup();            // Звук получения дополнительной жизни
}

void game_complete_level(void) {
    // Добавляем бонус за завершение уровня (как в BounceConst.java)
    game_add_score(EXIT_POINTS);

    // Обновить рекорды если нужно (как в оригинале BounceCanvas:179-185)
    save_update_records(g_game.selected_level, g_game.score);

    // Переход в экран завершения уровня (как в Java displayLevelComplete)
    g_game.state = STATE_LEVEL_COMPLETE;
}

// Универсальная функция деактивации кольца (перенесена из physics.c)
// Вспомогательная функция для сохранения флагов при установке нового ID
static void set_id_preserving_flags(int tx, int ty, uint8_t newID) {
    int currentTile = level_get_tile_at(tx, ty);
    // Извлекаем флаги (биты 6-7) из текущего тайла, сбрасывая ID (биты 0-5)
    // ~TILE_ID_MASK = ~0x3F = 0xC0 (биты 6-7)
    uint8_t flags = currentTile & ~TILE_ID_MASK; // Сохраняем все флаги кроме ID
    // Устанавливаем новый ID с сохранением старых флагов
    level_set_id(tx, ty, newID | flags);
}

static void deactivate_ring_pair(int x, int y, uint8_t tileID) {
    if (tileID >= tile_meta_count()) return;
    const TileMeta* meta = &tile_meta_db()[tileID];
    
    if (tileID >= 13 && tileID <= 14) {
        // Маленькие вертикальные кольца (ID=13-14)
        if (meta->orientation == ORIENT_VERT_TOP) {
            // Верхняя часть вертикального кольца (ID=13)
            set_id_preserving_flags(x, y, tileID + 4);     // верх → неактивный (13→17)
            set_id_preserving_flags(x, y + 1, tileID + 5); // низ → неактивный (13→18)
        } else if (meta->orientation == ORIENT_VERT_BOTTOM) {
            // Нижняя часть вертикального кольца (ID=14)
            set_id_preserving_flags(x, y, tileID + 4);     // низ → неактивный (14→18)
            set_id_preserving_flags(x, y - 1, tileID + 3); // верх → неактивный (14→17)
        }
    } else if (tileID >= 21 && tileID <= 22) {
        // Большие вертикальные кольца (ID=21-22) - логика как для 13-14
        if (tileID == 21) {
            // Верхняя часть большого вертикального кольца (ID=21)
            set_id_preserving_flags(x, y, 25);     // верх → неактивный (21→25)
            set_id_preserving_flags(x, y + 1, 26); // низ → неактивный (21→26)
        } else if (tileID == 22) {
            // Нижняя часть большого вертикального кольца (ID=22)
            set_id_preserving_flags(x, y, 26);     // низ → неактивный (22→26)
            set_id_preserving_flags(x, y - 1, 25); // верх → неактивный (22→25)
        }
    } else if (tileID >= 23 && tileID <= 24) {
        // Большие горизонтальные кольца (ID=23-24) - логика как для 15-16
        if (tileID == 23) {
            // Левая часть большого горизонтального кольца (ID=23)
            set_id_preserving_flags(x, y, 27);     // левая часть → неактивная левая (23→27)
            set_id_preserving_flags(x + 1, y, 28); // правая часть → неактивная правая (23→28)
        } else if (tileID == 24) {
            // Правая часть большого горизонтального кольца (ID=24)
            set_id_preserving_flags(x, y, 28);     // правая часть → неактивная правая (24→28)
            set_id_preserving_flags(x - 1, y, 27); // левая часть → неактивная левая (24→27)
        }
    } else if (meta->orientation == ORIENT_HORIZ_LEFT) {
        // Маленькие горизонтальные кольца - левая часть (ID=15)
        set_id_preserving_flags(x, y, 19);     // левая часть → неактивная левая (19)
        set_id_preserving_flags(x + 1, y, 20); // правая часть → неактивная правая (20)
    } else if (meta->orientation == ORIENT_HORIZ_RIGHT) {
        // Маленькие горизонтальные кольца - правая часть (ID=16)  
        set_id_preserving_flags(x, y, 20);     // правая часть → неактивная правая (20)
        set_id_preserving_flags(x - 1, y, 19); // левая часть → неактивная левая (19)
    }
}

// Новая функция для обработки события сбора кольца
void game_ring_collected(int tileX, int tileY, uint8_t tileID) {
    // 1. Деактивируем кольцо на карте
    deactivate_ring_pair(tileX, tileY, tileID);
    
    // 2. Добавляем очки и обновляем счетчик
    game_add_ring();
    
    // 3. Воспроизводим звук кольца (up.ott)
    sound_play_hoop();
    
    // 4. Если все кольца собраны - запускаем анимацию двери
    if (g_game.numRings >= g_level.totalRings) {
        game_exit_open();
    }
}

// === АНИМАЦИЯ ДВЕРИ ===

// Запуск анимации открытия двери (как в Java openExit())
void game_exit_open(void) {
    if (!g_exit_is_opening) {  // запускаем только если еще не открывается
        g_exit_is_opening = true;
        g_exit_animation_offset = 0;
        // В оригинале дверь открывается без звука
    }
}

// Сброс анимации двери при загрузке уровня
void game_exit_reset(void) {
    g_exit_animation_offset = 0;
    g_exit_is_opening = false;
}

// Получить текущее смещение анимации двери для рендера
int game_exit_anim_offset(void) {
    return g_exit_animation_offset;
}

// Проверить, завершена ли анимация открытия двери
bool game_exit_is_open(void) {
    return g_exit_animation_offset >= 24;
}

// Проверить, можно ли продолжить сохраненную игру
bool game_can_continue(void) {
    return g_game.saved_game_state == SAVED_GAME_IN_PROGRESS;
}
