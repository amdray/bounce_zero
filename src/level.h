// level.h - Парсер оригинальных уровней Bounce
#ifndef LEVEL_H
#define LEVEL_H

#include <psptypes.h>
#include "tile_table.h"
#include "png.h"  // Включаем png.h для полного определения texture_t
#include "types.h" // Для Player структуры

// Константы тайлов (из оригинального TileCanvas.java)

/* --- Ring foreground control (for proper draw order) --- */
#define RING_FG_QUEUE_MAX 128  // Максимальное количество колец для отложенного рендера (хватит для любого уровня)
void level_set_ring_fg_defer(int on);
void level_flush_ring_foreground(void);

/* --- Tile flags and masks --- */
// Структура байта тайла (8 бит):
// 7   6   5-0
// |   |   |---- ID тайла (0-63)
// |   |-------- Водный флаг
// |------------ Неиспользуемый бит
#define TILE_FLAG_WATER    0x40  // Флаг водного тайла (как в Java) - бит 6
#define TILE_ID_MASK       0x3F  // Маска для извлечения ID тайла (биты 0-5)
#define TILE_FLAGS_MASK    0x40  // Только флаг воды, без TILE_FLAG_MISC

/* ---------------------------------------------------------------------------
   ФОРМУЛА КОНВЕРТАЦИИ Java → PSP цветов:
   
   1. Java десятичное → hex: 545706₁₀ = 0x085300 (24-бит RGB)
   2. Извлечь каналы RGB: R=0x08, G=0x53, B=0xAA  
   3. Переставить в ABGR: A=0xFF, B=Java_B, G=Java_G, R=Java_R
   4. Результат PSP: 0xFFAA5308
   
   Примеры:
   - Java: 545706 = 0x085300 → R=08,G=53,B=AA → PSP: 0xFFAA5308 (HUD)
   - Java: 1073328 = 0x1060B0 → R=10,G=60,B=B0 → PSP: 0xFFB06010 (синий)
   
   Исключения (баги Java палитры):
   - Java: 11591920 → PSP: 0xFFE3D3A2 (не по формуле, legacy значение)
--------------------------------------------------------------------------- */
#define BACKGROUND_COLOUR     0xFFE3D3A2  // голубой
#define WATER_COLOUR          0xFFB06010  // синий
#define HUD_COLOUR            0xFFAA5308  // темно-синий HUD (Java 545706)
#define ABOUT_BACKGROUND_COLOUR 0xFFFBFF6C  // Особый фон для About экрана

// Exit door stripe colors (Java createExitImage)
#define EXIT_LIGHT_STRIPE_COLOUR  0xFF9E9DFC  // Java 16555422 = 0xFC9D9E RGB → ABGR
#define EXIT_DARK_STRIPE_COLOUR   0xFF3F3AE3  // Java 14891583 = 0xE33A3F RGB → ABGR
#define EXIT_FOURTH_STRIPE_COLOUR 0xFF8E84C2  // Java 12747918 = 0xC2848E RGB → ABGR

// Цвета для меню и текста
#define COLOR_SELECTION_BG    0xFF2135FF  // Красно-фиолетовый фон выделения
#define COLOR_TEXT_NORMAL     0xFF000000  // Черный обычный текст
#define COLOR_WHITE_ABGR      0xFFFFFFFF  // Белый цвет
#define COLOR_TEXT_SELECTED   0xFFFFFFFF  // Белый выделенный текст
#define COLOR_TEXT_HELP       0xFF333333  // Темно-серый текст подсказки
#define COLOR_DISABLED        0xFF808080  // Серый цвет для недоступных элементов
#define COLOR_TEXT_HIGHLIGHT  0xFF800000  // Темно-красный цвет для выделения (новый рекорд)
#define COLOR_BONUS_BAR       0xFF037FFF  // Оранжевый цвет полоски бонуса (Java: 16750611)
#define COLOR_BONUS_FRAME     0xFFFFFFFF  // Белая рамка полоски бонуса

/* --- Resource paths (following Java BounceConst pattern) --- */
#define TILESET_PATH          "icons/objects_nm.png"  // Основной атлас тайлов

// Кольца для сбора (13-28 в оригинале)

// Размеры
#define MAX_LEVEL_WIDTH 255
#define MAX_LEVEL_HEIGHT 255
#define MAX_MOVING_OBJECTS 16

// Структура движущегося объекта (шипов)
typedef struct {
    short topLeft[2];       // Верхний левый угол области движения (в тайлах)  
    short botRight[2];      // Нижний правый угол области движения (в тайлах)
    short direction[2];     // Направление движения по X,Y
    short offset[2];        // Текущее смещение внутри области (в пикселях)
} MovingObject;

// Структура уровня
typedef struct {
    int width;              // Ширина карты в тайлах
    int height;             // Высота карты в тайлах
    int startPosX;          // Стартовая позиция игрока X (в пикселях)
    int startPosY;          // Стартовая позиция игрока Y (в пикселях)
    int startTileX;         // Стартовая позиция игрока X (в тайлах)
    int startTileY;         // Стартовая позиция игрока Y (в тайлах)
    int ballSize;           // Размер мяча (0=маленький, 1=большой)
    int exitPosX;           // Позиция выхода X (в тайлах)
    int exitPosY;           // Позиция выхода Y (в тайлах)
    int totalRings;         // Общее количество колец для сбора
    
    // Движущиеся объекты
    int numMovingObjects;   // Количество движущихся объектов
    MovingObject movingObjects[MAX_MOVING_OBJECTS];
    
    // Карта тайлов
    short tileMap[MAX_LEVEL_HEIGHT][MAX_LEVEL_WIDTH];
} Level;

// Глобальный уровень
extern Level g_level;

// Функции для доступа к тайловому атласу
texture_t* level_get_tileset(void);
int level_get_tiles_per_row(void);

// Функции
int level_load_from_memory(const char* levelData, int dataSize);
int level_load_from_file(const char* filename);
int level_load_by_number(int levelNumber);
int level_get_tile_at(int tileX, int tileY);
void level_render_visible_area(int cameraX, int cameraY, int screenWidth, int screenHeight);

// Функции для движущихся объектов
void level_update_moving_objects(void);
int level_find_moving_object_at(int tileX, int tileY);
MovingObject* level_get_moving_object(int index);  // Получить движущийся объект по индексу

// Операции с тайлами карты (для событийной системы)
uint8_t level_get_id(int tx, int ty);              // Получить ID тайла (без флагов)
void level_set_id(int tx, int ty, uint8_t id);     // Установить ID тайла (с флагами)
void level_deactivate_old_checkpoint(void);        // Деактивировать старый чекпоинт (7->8)
void level_mark_checkpoint_active(int tx, int ty); // Активировать чекпоинт ((id&0x7F)|0x88)
void level_set_respawn(int tx, int ty);            // Установить новую точку респауна
void level_get_respawn(int* tx, int* ty);           // Получить текущую позицию респауна

// Cleanup
void level_cleanup(void);

#endif