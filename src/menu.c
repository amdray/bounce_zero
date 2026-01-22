#include "menu.h"
#include "graphics.h"
#include "input.h"
#include "types.h"
#include "level.h"
#include "game.h"
#include "png.h"
#include "local.h"
#include "local_extra.h"
#include <pspctrl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Нужен доступ к глобальному состоянию игры
extern Game g_game;

// Перечисление пунктов меню для безопасности типов
typedef enum {
    MENU_CONTINUE = 0,      // Продолжить игру
    MENU_NEW_GAME = 1,      // Новая игра
    MENU_SELECT_LEVEL = 2,  // Выбор уровня
    MENU_HIGH_SCORE = 3,    // Рекорды
    MENU_INSTRUCTIONS = 4,  // Инструкции
    MENU_ITEMS_COUNT        // Общее количество пунктов
} MenuItem;

typedef enum {
    MENU_LABEL_LOCAL_ID = 0,
    MENU_LABEL_SELECT_LEVEL = 1
} MenuLabelType;

typedef struct {
    MenuLabelType label_type;
    int label_id;
    int y;
    u32 color_unselected;
} MenuItemDef;

enum {
    INSTR_MAX_LINES = 16,
    INSTR_MAX_LINE_LENGTH = 256
};

// Статическая переменная для отслеживания текущей части инструкций (0-5)
static int current_instruction_page = 0;
static char s_instruction_lines[INSTRUCTIONS_TOTAL_PAGES][INSTR_MAX_LINES][INSTR_MAX_LINE_LENGTH];
static int s_instruction_line_counts[INSTRUCTIONS_TOTAL_PAGES];
static int s_instruction_cached[INSTRUCTIONS_TOTAL_PAGES];
static char s_instruction_lang[8];

typedef struct {
    int x;
    int y;
    int w;
    int h;
    int center_x;
} ModalPanel;

// Разбивает текст по словам на строки, не превышающие max_width пикселей
static int wrap_text_lines(const char* text, int max_width, int font_height,
                           char lines[][INSTR_MAX_LINE_LENGTH], int max_lines) {
    if (!text || max_lines <= 0) {
        return 0;
    }

    int line_count = 0;
    char line[INSTR_MAX_LINE_LENGTH];
    int line_len = 0;
    line[0] = '\0';

    const char* p = text;
    while (*p) {
        while (*p == ' ') {
            p++;
        }
        if (!*p) {
            break;
        }

        char word[INSTR_MAX_LINE_LENGTH];
        int word_len = 0;
        while (*p && *p != ' ' && word_len < INSTR_MAX_LINE_LENGTH - 1) {
            word[word_len++] = *p++;
        }
        word[word_len] = '\0';

        if (word_len == 0) {
            continue;
        }

        if (line_len == 0) {
            strncpy(line, word, sizeof(line) - 1);
            line[sizeof(line) - 1] = '\0';
            line_len = (int)strlen(line);
        } else {
            char candidate[INSTR_MAX_LINE_LENGTH];
            if (snprintf(candidate, sizeof(candidate), "%s %s", line, word) >= (int)sizeof(candidate)) {
                if (line_count >= max_lines) {
                    return line_count;
                }
                size_t len = strlen(line);
                if (len >= INSTR_MAX_LINE_LENGTH) {
                    len = INSTR_MAX_LINE_LENGTH - 1;
                }
                memcpy(lines[line_count], line, len);
                lines[line_count][len] = '\0';
                line_count++;
                strncpy(line, word, sizeof(line) - 1);
                line[sizeof(line) - 1] = '\0';
                line_len = (int)strlen(line);
            } else if (graphics_measure_text(candidate, font_height) <= max_width) {
                strncpy(line, candidate, sizeof(line) - 1);
                line[sizeof(line) - 1] = '\0';
                line_len = (int)strlen(line);
            } else {
                if (line_count >= max_lines) {
                    return line_count;
                }
                size_t len = strlen(line);
                if (len >= INSTR_MAX_LINE_LENGTH) {
                    len = INSTR_MAX_LINE_LENGTH - 1;
                }
                memcpy(lines[line_count], line, len);
                lines[line_count][len] = '\0';
                line_count++;
                strncpy(line, word, sizeof(line) - 1);
                line[sizeof(line) - 1] = '\0';
                line_len = (int)strlen(line);
            }
        }
    }

    if (line_len > 0 && line_count < max_lines) {
        size_t len = strlen(line);
        if (len >= INSTR_MAX_LINE_LENGTH) {
            len = INSTR_MAX_LINE_LENGTH - 1;
        }
        memcpy(lines[line_count], line, len);
        lines[line_count][len] = '\0';
        line_count++;
    }

    return line_count;
}

static void invalidate_instruction_cache(void) {
    for (int i = 0; i < INSTRUCTIONS_TOTAL_PAGES; ++i) {
        s_instruction_cached[i] = 0;
        s_instruction_line_counts[i] = 0;
    }
}

static void ensure_instruction_cache(int page, int max_width, int font_height) {
    const char* lang = local_get_lang();
    if (strcmp(s_instruction_lang, lang) != 0) {
        strncpy(s_instruction_lang, lang, sizeof(s_instruction_lang) - 1);
        s_instruction_lang[sizeof(s_instruction_lang) - 1] = '\0';
        invalidate_instruction_cache();
    }

    if (page < 0 || page >= INSTRUCTIONS_TOTAL_PAGES) {
        return;
    }

    if (s_instruction_cached[page]) {
        return;
    }

    const char* text = local_get_text(QHJ_BOUN_INSTRUCTIONS_PART_1 + page);
    s_instruction_line_counts[page] = wrap_text_lines(text, max_width, font_height,
                                                      s_instruction_lines[page], INSTR_MAX_LINES);
    s_instruction_cached[page] = 1;
}

static const MenuItemDef k_menu_items[MENU_ITEMS_COUNT] = {
    { MENU_LABEL_LOCAL_ID, QTJ_BOUN_CONTINUE,     105, COLOR_TEXT_NORMAL },
    { MENU_LABEL_LOCAL_ID, QTJ_BOUN_NEW_GAME,     130, COLOR_TEXT_NORMAL },
    { MENU_LABEL_SELECT_LEVEL, 0,                155, COLOR_TEXT_NORMAL },
    { MENU_LABEL_LOCAL_ID, QTJ_BOUN_HIGH_SCORES,  180, COLOR_TEXT_NORMAL },
    { MENU_LABEL_LOCAL_ID, QTJ_BOUN_INSTRUCTIONS, 205, COLOR_TEXT_NORMAL }
};

static const char* menu_item_label(const MenuItemDef* item) {
    if (item->label_type == MENU_LABEL_SELECT_LEVEL) {
        return local_text_select_level();
    }
    return local_get_text(item->label_id);
}

static ModalPanel draw_modal_panel(u32 background_color, u32 panel_color, const char* title, int title_font, u32 title_color) {
    ModalPanel panel;

    panel.w = 240;
    panel.h = 128;
    panel.x = (SCREEN_WIDTH - panel.w) / 2;
    panel.y = (SCREEN_HEIGHT - panel.h) / 2;
    panel.center_x = SCREEN_WIDTH / 2;

    graphics_draw_rect(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, background_color);
    graphics_draw_rect((float)panel.x, (float)panel.y, (float)panel.w, (float)panel.h, panel_color);

    if (title) {
        int text_y = panel.y + 5;
        int w = graphics_measure_text(title, title_font);
        int x = panel.center_x - (w / 2);
        graphics_draw_text((float)x, (float)text_y, title, title_color, title_font);
    }

    return panel;
}

void menu_init(void) {
    // Инициализация меню (пока пустая)
}

void menu_update(void) {
    // Убедиться что стартовая позиция не на недоступном Continue
    if(g_game.menu_selection == MENU_CONTINUE && !game_can_continue()) {
        g_game.menu_selection = MENU_NEW_GAME; // Начать с New Game
    }
    
    // Навигация по меню (5 пунктов: 0,1,2,3,4)
    if(input_pressed(PSP_CTRL_UP) && g_game.menu_selection > 0) {
        g_game.menu_selection--;
        // Если Continue недоступен и мы попали на него - перейти на New Game
        if(g_game.menu_selection == MENU_CONTINUE && !game_can_continue()) {
            g_game.menu_selection = MENU_NEW_GAME;
        }
    }
    if(input_pressed(PSP_CTRL_DOWN) && g_game.menu_selection < MENU_ITEMS_COUNT-1) {
        g_game.menu_selection++;
    }
    
    // Выбор пункта меню
    if(input_pressed(PSP_CTRL_CROSS)) {
        if(g_game.menu_selection == MENU_CONTINUE) {
            // CONTINUE - продолжить сохраненную игру
            if(game_can_continue()) {
                // Просто вернуться в игру - состояние уже сохранено
                g_game.state = STATE_GAME;
            }
        } else if(g_game.menu_selection == MENU_NEW_GAME) {
            // NEW GAME - новая игра с уровня 1
            game_start_level(1, GAME_START_FRESH);
        } else if(g_game.menu_selection == MENU_SELECT_LEVEL) {
            // SELECT LEVEL
            g_game.state = STATE_LEVEL_SELECT;
        } else if(g_game.menu_selection == MENU_HIGH_SCORE) {
            // HIGH SCORE - экран рекордов
            g_game.state = STATE_HIGH_SCORE;
        } else if(g_game.menu_selection == MENU_INSTRUCTIONS) {
            // INSTRUCTIONS - экран правил/инструкций
            g_game.state = STATE_INSTRUCTIONS;
        }
    }
}

void instructions_update(void) {
    // Навигация между частями
    if(input_pressed(PSP_CTRL_LEFT) && current_instruction_page > 0) {
        current_instruction_page--;
    }
    if(input_pressed(PSP_CTRL_RIGHT) && current_instruction_page < INSTRUCTIONS_TOTAL_PAGES - 1) {
        current_instruction_page++;
    }

    // Возврат в меню по любой кнопке
    if(input_pressed(PSP_CTRL_CROSS) || input_pressed(PSP_CTRL_CIRCLE) || input_pressed(PSP_CTRL_START)) {
        current_instruction_page = 0; // Сброс при выходе
        g_game.state = STATE_MENU;
    }
}

void menu_render(void) {

    // UI без текстурной модуляции
    graphics_begin_plain();
    const int center_x = SCREEN_WIDTH / 2;
    const int title_y  = MAIN_MENU_TITLE_Y;
    const int font_height = 23;
    const int   bg_height = MAIN_MENU_SELECTION_BG_HEIGHT;

    // Заголовок (по центру)
    {
        const char* title = local_get_text(QTJ_BOUN_GAME_NAME); // ID 10, как в оригинале BounceUI.java:65
        int w = graphics_measure_text(title, font_height);
        int x = center_x - (w / 2);
        graphics_draw_text((float)x, (float)title_y, title, COLOR_SELECTION_BG, font_height);
    }
    
    // Данные пунктов меню (5 пунктов с равномерным расстоянием 25px)
    for (int i = 0; i < MENU_ITEMS_COUNT; ++i) {
        const int selected = (g_game.menu_selection == i);
        const MenuItemDef* item = &k_menu_items[i];
        const char* label = menu_item_label(item);
        
        // Continue доступен только при наличии сохраненной игры
        const int is_continue_unavailable = (i == MENU_CONTINUE && !game_can_continue());
        
        u32 color;
        if (is_continue_unavailable) {
            color = COLOR_DISABLED; // Серый цвет для недоступного пункта
        } else {
            color = selected ? COLOR_TEXT_SELECTED : item->color_unselected;
        }
        int w = graphics_measure_text(label, font_height);
        int x = center_x - (w / 2);
        int y = item->y;

        // Фон под выбранным пунктом — строго по ширине текста с одинаковыми полями
        if (selected) {
            const int padding_x = (int)MAIN_MENU_PADDING_X;
            const int padding_y = (int)MAIN_MENU_PADDING_Y;
            graphics_draw_rect((float)(x - padding_x), (float)(y - padding_y),
                               (float)(w + 2 * padding_x), (float)bg_height, COLOR_SELECTION_BG);
        }

        // Текст пункта
        graphics_draw_text((float)x, (float)y, label, color, font_height);
    }

    // Будущие иконки меню
    // graphics_begin_textured();
    // TODO: render menu icons here
}

void instructions_render(void) {
    graphics_begin_plain();

    // Шапка "Инструкции" (шрифт 23, getText(13), BounceUI.java:107)
    const char* title = local_get_text(QTJ_BOUN_INSTRUCTIONS);
    ModalPanel panel = draw_modal_panel(BACKGROUND_COLOUR, COLOR_WHITE_ABGR, title, 23, COLOR_TEXT_NORMAL);
    int text_y = panel.y + 5 + 23 + 10;  // После шапки + отступ

    // Текст инструкций внутри панели
    int max_width = panel.w - 40; // Ширина панели минус отступы по 20px с каждой стороны
    int line_height = 14; // Уменьшенная высота строки для помещения в панель

    if (current_instruction_page < INSTRUCTIONS_TOTAL_PAGES) {
        ensure_instruction_cache(current_instruction_page, max_width, 12);
        int line_count = s_instruction_line_counts[current_instruction_page];
        for (int i = 0; i < line_count; ++i) {
            int line_y = text_y + i * line_height;
            graphics_draw_text((float)(panel.x + 20), (float)line_y,
                               s_instruction_lines[current_instruction_page][i],
                               COLOR_TEXT_NORMAL, 12);
        }
    }

    // Индикатор страницы под белой панелью, по центру
    text_y = panel.y + panel.h + 10;  // 10px под панелью
    char page_info[64];
    snprintf(page_info, sizeof(page_info), "Часть %d из %d", current_instruction_page + 1, INSTRUCTIONS_TOTAL_PAGES);
    int page_w = graphics_measure_text(page_info, 9);
    graphics_draw_text((float)(panel.center_x - (page_w / 2)), (float)text_y, page_info, COLOR_TEXT_NORMAL, 9);
}

void level_select_update(void) {
    const int columns = LEVEL_GRID_COLUMNS;
    const int rows = (MAX_LEVEL + columns - 1) / columns;

    // Навигация вверх/вниз с учетом центрирования неполной строки
    int current_row = (g_game.selected_level - 1) / columns;
    int current_row_start = current_row * columns + 1;
    int remaining_current = MAX_LEVEL - current_row * columns;
    int items_current = (remaining_current < columns) ? remaining_current : columns;
    int offset_current = (columns - items_current) / 2;
    int col_in_row = g_game.selected_level - current_row_start;
    int visual_col = col_in_row + offset_current;

    // Навигация по уровням (сетка 5x3: 1-5, 6-10, 11)
    if(input_pressed(PSP_CTRL_LEFT) && g_game.selected_level > 1) {
        g_game.selected_level--;
    }
    if(input_pressed(PSP_CTRL_RIGHT) && g_game.selected_level < MAX_LEVEL) {
        g_game.selected_level++;
    }
    if(input_pressed(PSP_CTRL_UP)) {
        if(current_row > 0) {
            int target_row = current_row - 1;
            int target_row_start = target_row * columns + 1;
            int remaining_target = MAX_LEVEL - target_row * columns;
            int items_target = (remaining_target < columns) ? remaining_target : columns;
            int offset_target = (columns - items_target) / 2;
            int target_col = visual_col - offset_target;
            if (target_col < 0) target_col = 0;
            if (target_col >= items_target) target_col = items_target - 1;
            g_game.selected_level = target_row_start + target_col;
        }
    }
    if(input_pressed(PSP_CTRL_DOWN)) {
        if(current_row < rows - 1) {
            int target_row = current_row + 1;
            int target_row_start = target_row * columns + 1;
            int remaining_target = MAX_LEVEL - target_row * columns;
            int items_target = (remaining_target < columns) ? remaining_target : columns;
            int offset_target = (columns - items_target) / 2;
            int target_col = visual_col - offset_target;
            if (target_col < 0) target_col = 0;
            if (target_col >= items_target) target_col = items_target - 1;
            g_game.selected_level = target_row_start + target_col;
        }
    }
    
    // Ограничиваем диапазон 1-11
    if(g_game.selected_level < 1) g_game.selected_level = 1;
    if(g_game.selected_level > MAX_LEVEL) g_game.selected_level = MAX_LEVEL;
    
    // Выбор уровня
    if(input_pressed(PSP_CTRL_CROSS)) {
        game_start_level(g_game.selected_level, GAME_START_SELECTED);
    }
    
    // Возврат в меню
    if(input_pressed(PSP_CTRL_CIRCLE)) {
        g_game.state = STATE_MENU;
    }
}

void level_select_render(void) {
    const int center_x = SCREEN_WIDTH / 2;
    
    // UI цифры из битмапа — без текстурной модуляции
    graphics_begin_plain();
// Заголовок
    {
        const char* title = local_text_select_level();
        int w = graphics_measure_text(title, 23);
        int x = center_x - (w / 2);
        graphics_draw_text((float)x, (float)LEVEL_SELECT_TITLE_Y, title, COLOR_TEXT_NORMAL, 23);
    }
    
    // Размеры для сетки уровней
    int start_x = LEVEL_GRID_START_X;
    int start_y = LEVEL_GRID_START_Y;
    int cell_width = LEVEL_CELL_WIDTH;
    int cell_height = LEVEL_CELL_HEIGHT;
    int spacing_x = LEVEL_SPACING_X;
    int spacing_y = LEVEL_SPACING_Y;
    int columns = LEVEL_GRID_COLUMNS;
    int rows = (MAX_LEVEL + columns - 1) / columns;
    int full_row_width = (columns - 1) * spacing_x;
    
    // Рисуем уровни 1..MAX_LEVEL в сетке с центрированием последней строки
    for(int row = 0; row < rows; row++) {
        int row_start_level = row * columns + 1;
        int remaining = MAX_LEVEL - row * columns;
        int items_in_row = remaining < columns ? remaining : columns;
        int row_width = (items_in_row - 1) * spacing_x;
        int row_offset = (full_row_width - row_width) / 2;

        for(int col = 0; col < items_in_row; col++) {
            int level = row_start_level + col;
            int x = start_x + row_offset + col * spacing_x;
            int y = start_y + row * spacing_y;

            // Цветной фон для выбранного уровня (красный как в "Bounce")
            if(level == g_game.selected_level) {
                graphics_draw_rect((float)(x - 5), (float)(y - 5), (float)cell_width, (float)cell_height, COLOR_SELECTION_BG);
            }

            // Номер уровня
            char level_text[4];
            snprintf(level_text, sizeof(level_text), "%d", level);

            u32 color = (level == g_game.selected_level) ? COLOR_TEXT_SELECTED : COLOR_TEXT_NORMAL;
            graphics_draw_text((float)x, (float)y, level_text, color, 23);
        }
    }
}

void high_score_update(void) {
    // Возврат в меню (как в оригинале mBackCmd)
    if(input_pressed(PSP_CTRL_CROSS) || 
       input_pressed(PSP_CTRL_CIRCLE) || 
       input_pressed(PSP_CTRL_START)) {
        g_game.state = STATE_MENU;
    }
}

void high_score_render(void) {
    SaveData* save = save_get_data();

    // UI без текстурной модуляции
    graphics_begin_plain();

    // Шапка "HIGH SCORES" (шрифт 23, как в Java Form header)
    const char* title = local_get_text(QTJ_BOUN_HIGH_SCORES);
    ModalPanel panel = draw_modal_panel(BACKGROUND_COLOUR, COLOR_WHITE_ABGR, title, 23, COLOR_TEXT_NORMAL);
    int text_y = panel.y + 5 + 23 + 10;  // После шапки + отступ

    // Пустая строка для симметричности с level_complete (там "Level X completed!")
    text_y += 9 + 20;  // Высота шрифта 9 + отступ 20px

    // Фиксированная позиция для счёта (одинаковая во всех окнах)
    text_y = panel.y + 60;

    // Лучший счёт (красный, шрифт 24)
    {
        int w = graphics_measure_number(save->best_score);
        graphics_draw_number((float)(panel.center_x - (w / 2)), (float)text_y, save->best_score, COLOR_SELECTION_BG);
    }

    // Кнопка "X - Continue/Return" - фиксированная позиция внизу панели
    text_y = panel.y + panel.h - 25;  // 25px от низа панели
    {
        const char* continue_text = local_get_text(QTJ_BOUN_BACK);
        int w = graphics_measure_text(continue_text, 9);

        // Полная ширина блока: иконка (12px диаметр) + отступ (4px) + текст
        int total_width = 12 + 4 + w;
        int start_x = panel.center_x - (total_width / 2);

        // Иконка кнопки X (текстом)
        {
            const char* icon = "(X)";
            int icon_w = graphics_measure_text(icon, 9);
            int icon_x = start_x + (12 - icon_w) / 2;
            graphics_draw_text((float)icon_x, (float)text_y, icon, COLOR_TEXT_NORMAL, 9);
        }

        // Текст кнопки
        graphics_draw_text((float)(start_x + 12 + 4), (float)text_y, continue_text, COLOR_TEXT_NORMAL, 9);
    }

}

void game_over_update(void) {
    // Возврат в меню только по X (Continue) как в оригинале
    if(input_pressed(PSP_CTRL_CROSS)) {
        g_game.state = STATE_MENU;
    }
}

void game_over_render(void) {
    // UI без текстурной модуляции
    graphics_begin_plain();
    
    // Шапка "GAME OVER" (шрифт 23, Local.getText(11), BounceUI.java:127-128)
    const char* title = local_get_text(QTJ_BOUN_GAME_OVER);
    ModalPanel panel = draw_modal_panel(BACKGROUND_COLOUR, COLOR_WHITE_ABGR, title, 23, COLOR_TEXT_NORMAL);
    int text_y = panel.y + 5 + 23 + 10;  // После шапки + отступ

    // Сообщение о новом рекорде (BounceUI.java:131-133) - для симметричности с level_complete
    if (g_game.new_best_score) {
        const char* new_record = local_get_text(QTJ_BOUN_NEW_HIGH_SCORE);
        int w = graphics_measure_text(new_record, 9);
        graphics_draw_text((float)(panel.center_x - (w / 2)), (float)text_y, new_record, COLOR_SELECTION_BG, 9);
    }
    text_y += 9 + 20;  // Пропуск для симметричности (высота строки + отступ)

    // Фиксированная позиция для счёта (одинаковая во всех окнах)
    text_y = panel.y + 60;

    // Только число счёта, без "Score:" - красный, шрифт 24 (BounceUI.java:136)
    {
        int w = graphics_measure_number(g_game.score);
        graphics_draw_number((float)(panel.center_x - (w / 2)), (float)text_y, g_game.score, COLOR_SELECTION_BG);
    }

    // Кнопка "X - OK" - фиксированная позиция внизу панели (Local.getText(19), BounceUI.java:125)
    text_y = panel.y + panel.h - 25;  // 25px от низа панели
    {
        const char* ok_text = local_get_text(QTJ_BOUN_OK);
        int w = graphics_measure_text(ok_text, 9);

        // Полная ширина блока: иконка (12px диаметр) + отступ (4px) + текст
        int total_width = 12 + 4 + w;
        int start_x = panel.center_x - (total_width / 2);

        // Иконка кнопки X (текстом)
        {
            const char* icon = "(X)";
            int icon_w = graphics_measure_text(icon, 9);
            int icon_x = start_x + (12 - icon_w) / 2;
            graphics_draw_text((float)icon_x, (float)text_y, icon, COLOR_TEXT_NORMAL, 9);
        }

        // Текст кнопки
        graphics_draw_text((float)(start_x + 12 + 4), (float)text_y, ok_text, COLOR_TEXT_NORMAL, 9);
    }
}

// =============================================================================
// LEVEL COMPLETE ЭКРАН
// =============================================================================

void level_complete_update(void) {
    // X = продолжить (следующий уровень или Game Over)
    if(input_pressed(PSP_CTRL_CROSS)) {
        // Проверяем если не последний уровень (до 11) - переходим на следующий
        if(g_game.selected_level < MAX_LEVEL) {
            game_start_level(g_game.selected_level + 1, GAME_START_NEXT);
        } else {
            // Последний уровень пройден - Game Over экран (как в оригинале BounceCanvas:539)
            g_game.saved_game_state = SAVED_GAME_COMPLETED;
            g_game.state = STATE_GAME_OVER;
        }
    }
    // O = выйти в меню
    if(input_pressed(PSP_CTRL_CIRCLE)) {
        g_game.state = STATE_MENU;
    }
}

void level_complete_render(void) {
    // UI без текстурной модуляции
    graphics_begin_plain();

    // Шапка "Поздравления!" (шрифт 23)
    const char* title = local_get_text(QTJ_BOUN_CONGRATULATIONS);
    ModalPanel panel = draw_modal_panel(BACKGROUND_COLOUR, COLOR_WHITE_ABGR, title, 23, COLOR_TEXT_NORMAL);
    int text_y = panel.y + 5 + 23 + 10;  // После шапки + отступ

    // "Level X completed!" (Local.getText(15) с параметром, BounceUI.java:150)
    {
        char level_str[16];
        snprintf(level_str, sizeof(level_str), "%d", g_game.selected_level);
        const char* params[] = { level_str };
        const char* level_title = local_get_text_with_params(QTJ_BOUN_LEVEL_COMPLETED, params, 1);
        int w = graphics_measure_text(level_title, 9);
        graphics_draw_text((float)(panel.center_x - (w / 2)), (float)text_y, level_title, COLOR_TEXT_NORMAL, 9);
    }

    // Счёт - красный, шрифт 24, фиксированная позиция (как в high_score) (BounceUI.java:152)
    text_y = panel.y + 60;  // Фиксированная позиция (одинаковая в high_score и level_complete)
    {
        int w = graphics_measure_number(g_game.score);
        graphics_draw_number((float)(panel.center_x - (w / 2)), (float)text_y, g_game.score, COLOR_SELECTION_BG);
    }

    // Кнопка "X - Continue" - фиксированная позиция внизу панели (Local.getText(8), BounceUI.java:147)
    text_y = panel.y + panel.h - 25;  // 25px от низа панели (как в Game Over)
    {
        const char* continue_text = local_get_text(QTJ_BOUN_CONTINUE);
        int w = graphics_measure_text(continue_text, 9);

        // Полная ширина блока: иконка (12px диаметр) + отступ (4px) + текст
        int total_width = 12 + 4 + w;
        int start_x = panel.center_x - (total_width / 2);

        // Иконка кнопки X (текстом)
        {
            const char* icon = "(X)";
            int icon_w = graphics_measure_text(icon, 9);
            int icon_x = start_x + (12 - icon_w) / 2;
            graphics_draw_text((float)icon_x, (float)text_y, icon, COLOR_TEXT_NORMAL, 9);
        }

        // Текст кнопки
        graphics_draw_text((float)(start_x + 12 + 4), (float)text_y, continue_text, COLOR_TEXT_NORMAL, 9);
    }
}

void menu_cleanup(void) {
    // Очистка ресурсов меню (пока пустая)
}

// =============================================================================
// НОВЫЙ УНИФИЦИРОВАННЫЙ API
// =============================================================================

// Универсальная функция обновления меню
void menu_update_by_type(menu_type_t type) {
    switch (type) {
        case MENU_TYPE_MAIN:
            menu_update();
            break;
        case MENU_TYPE_LEVEL_SELECT:
            level_select_update();
            break;
        case MENU_TYPE_HIGH_SCORE:
            high_score_update();
            break;
        case MENU_TYPE_GAME_OVER:
            game_over_update();
            break;
        case MENU_TYPE_LEVEL_COMPLETE:
            level_complete_update();
            break;
        case MENU_TYPE_INSTRUCTIONS:
            instructions_update();
            break;
    }
}

// Универсальная функция рендеринга меню
void menu_render_by_type(menu_type_t type) {
    switch (type) {
        case MENU_TYPE_MAIN:
            menu_render();
            break;
        case MENU_TYPE_LEVEL_SELECT:
            level_select_render();
            break;
        case MENU_TYPE_HIGH_SCORE:
            high_score_render();
            break;
        case MENU_TYPE_GAME_OVER:
            game_over_render();
            break;
        case MENU_TYPE_LEVEL_COMPLETE:
            level_complete_render();
            break;
        case MENU_TYPE_INSTRUCTIONS:
            instructions_render();
            break;
    }
}

// Хелпер: преобразование game state в menu type
menu_type_t menu_get_type_from_game_state(int game_state) {
    switch (game_state) {
        case STATE_MENU:
            return MENU_TYPE_MAIN;
        case STATE_LEVEL_SELECT:
            return MENU_TYPE_LEVEL_SELECT;
        case STATE_HIGH_SCORE:
            return MENU_TYPE_HIGH_SCORE;
        case STATE_GAME_OVER:
            return MENU_TYPE_GAME_OVER;
        case STATE_LEVEL_COMPLETE:
            return MENU_TYPE_LEVEL_COMPLETE;
        case STATE_INSTRUCTIONS:
            return MENU_TYPE_INSTRUCTIONS;
        default:
            return MENU_TYPE_MAIN; // Fallback
    }
}
