#ifndef GAME_H
#define GAME_H

#include "types.h"

void game_init(void);
void game_update(void);
void game_render(void);
void game_shutdown(void);
void game_reset_camera(void);

typedef enum {
    GAME_START_FRESH = 0,
    GAME_START_SELECTED = 1,
    GAME_START_NEXT = 2
} game_start_mode_t;

void game_start_level(int level_number, game_start_mode_t mode);

typedef enum {
    GAME_TICK_VARIABLE = 0,
    GAME_TICK_FIXED = 1
} game_tick_mode_t;

typedef struct {
    void (*update)(void);
    void (*render)(void);
    game_tick_mode_t tick_mode;
} game_state_handler_t;

const game_state_handler_t* game_get_state_handler(GameState state);
void game_state_update(void);
void game_state_render(void);

// Анимация двери
void game_exit_open(void);
void game_exit_reset(void);
int game_exit_anim_offset(void);
bool game_exit_is_open(void);

// Проверка состояния сохраненной игры
bool game_can_continue(void);

extern Game g_game;

#endif
