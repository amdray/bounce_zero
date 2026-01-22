#ifndef MENU_H
#define MENU_H

#include "types.h"

// Типы меню для унификации API
typedef enum {
    MENU_TYPE_MAIN = 0,
    MENU_TYPE_LEVEL_SELECT,
    MENU_TYPE_HIGH_SCORE,
    MENU_TYPE_GAME_OVER,
    MENU_TYPE_LEVEL_COMPLETE,
    MENU_TYPE_INSTRUCTIONS
} menu_type_t;

// Константы расположения элементов меню

// Главное меню
#define MAIN_MENU_TITLE_Y        50        // Позиция заголовка "Bounce"
#define MAIN_MENU_PADDING_X      5         // Горизонтальный отступ выделения
#define MAIN_MENU_PADDING_Y      0         // Вертикальный отступ выделения (0 = верх прямоугольника совпадает с y текста)
#define MAIN_MENU_SELECTION_BG_HEIGHT 25   // Высота подложки выделения

// Экран выбора уровня
#define LEVEL_SELECT_TITLE_Y 30       // Позиция заголовка "SELECT LEVEL"
#define LEVEL_GRID_START_X   140      // Начальная X позиция сетки уровней
#define LEVEL_GRID_START_Y   80       // Начальная Y позиция сетки уровней  
#define LEVEL_CELL_WIDTH     40       // Ширина ячейки уровня
#define LEVEL_CELL_HEIGHT    30       // Высота ячейки уровня
#define LEVEL_SPACING_X      50       // Расстояние между ячейками по X
#define LEVEL_SPACING_Y      40       // Расстояние между ячейками по Y
#define LEVEL_GRID_COLUMNS   5        // Количество колонок в сетке уровней
#define LEVEL_HELP_Y         250      // Позиция текста помощи

// Инициализация меню (загрузка текстур и т.д.)
void menu_init(void);

// Универсальное обновление меню по типу (заменяет все *_update функции)
void menu_update_by_type(menu_type_t type);

// Универсальный рендеринг меню по типу (заменяет все *_render функции)
void menu_render_by_type(menu_type_t type);

// Хелпер: преобразование game state в menu type
menu_type_t menu_get_type_from_game_state(int game_state);

// Очистка ресурсов меню
void menu_cleanup(void);

// УСТАРЕВШИЕ ФУНКЦИИ (для совместимости, будут удалены)
void menu_update(void);
void menu_render(void);
void level_select_update(void);
void level_select_render(void);
void high_score_update(void);
void high_score_render(void);
void game_over_update(void);
void game_over_render(void);
void level_complete_update(void);
void level_complete_render(void);
void instructions_update(void);
void instructions_render(void);

#endif
