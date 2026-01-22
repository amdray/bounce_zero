#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "tile_table.h"
#include "png.h"  // Для texture_t

#ifdef __cplusplus
extern "C" {
#endif

// Состояния игры
typedef enum {
    STATE_SPLASH_NOKIA,  // Nokia Games splash screen
    STATE_SPLASH,        // Bounce splash screen
    STATE_MENU,
    STATE_LEVEL_SELECT,  // Выбор уровня
    STATE_GAME,
    STATE_HIGH_SCORE,     // Экран рекордов
    STATE_INSTRUCTIONS,   // Экран инструкций/правил
    STATE_LEVEL_COMPLETE, // Завершение уровня (как в Java displayLevelComplete)
    STATE_GAME_OVER,      // Game Over экран (как в Java displayGameOver)
    STATE_EXIT
} GameState;

// Состояние сохраненной игры для Continue
typedef enum {
    SAVED_GAME_NONE,        // Нет сохраненной игры
    SAVED_GAME_IN_PROGRESS, // Игра приостановлена (можно Continue)
    SAVED_GAME_COMPLETED    // Игра завершена
} SavedGameState;

// ============================================================================
// ИГРОВЫЕ КОНСТАНТЫ (из BounceConst.java)
// ============================================================================

// Размеры спрайтов (в пикселях)
#define NORMAL_SIZE 12      // Обычный размер мяча
#define HALF_NORMAL_SIZE 6
#define ENLARGED_SIZE 16    // Увеличенный размер мяча
#define HALF_ENLARGED_SIZE 8
#define POPPED_SIZE 12      // Размер лопнувшего мяча (совпадает с обычным по дизайну)
#define HALF_POPPED_SIZE 6

// Физика прыжков
#define JUMP_STRENGTH -67           // Сила обычного прыжка
#define JUMP_STRENGTH_INC -10       // Инкремент силы прыжка
#define JUMP_BONUS_STRENGTH -80     // Сила бонусного прыжка

// Гравитация на суше
#define NORMAL_GRAVITY_ACCELL 4     // Ускорение гравитации маленький мяч
#define LARGE_GRAVITY_ACCELL 3      // Ускорение гравитации большой мяч
#define NORMAL_MAX_GRAVITY 80       // Максимальная гравитация маленький мяч
#define LARGE_MAX_GRAVITY 38        // Максимальная гравитация большой мяч

// Подводная физика
#define UWATER_MAX_GRAVITY 42           // Подводная гравитация маленький мяч
#define UWATER_GRAVITY_ACCELL 6         // Ускорение в воде маленький мяч
#define UWATER_LARGE_MAX_GRAVITY -30    // Подводная гравитация большой мяч (всплывает!)
#define LARGE_UWATER_GRAVITY_ACCELL -2  // Ускорение в воде большой мяч
#define BASE_GRAVITY 10                 // Базовое значение гравитации

// Горизонтальное движение
#define MAX_TOTAL_SPEED 150         // Максимальная общая скорость
#define HORZ_ACCELL 6              // Горизонтальное ускорение
#define FRICTION_DECELL 4          // Замедление от трения
#define MAX_HORZ_SPEED 50          // Максимальная горизонтальная скорость
#define MAX_HORZ_BONUS_SPEED 100   // Максимальная скорость с бонусом

// Отскоки и коллизии
#define MIN_BOUNCE_SPEED 10         // Минимальная скорость отскока
#define ROOF_COLLISION_SPEED 20     // Скорость при столкновении с потолком

// Бонусы
#define BONUS_DURATION 300          // Длительность бонусов в кадрах

// Анимация
#define POPPED_FRAMES 5             // Длительность анимации лопания мяча в кадрах

// Коллизии
#define THIN_TILE_SIZE 4            // Размер тонкого тайла для точных коллизий

// Splash экраны (соответствуют BounceConst.java)
#define SPLASH_NAME_NOKIA     "icons/nokiagames.png"    // Nokia Games splash
#define SPLASH_NAME_BOUNCE    "icons/bouncesplash.png"  // Bounce splash

// Звуковые файлы (соответствуют BounceConst.java)
#define SOUND_HOOP_NAME       "sounds/up.ott"     // Звук сбора кольца
#define SOUND_PICKUP_NAME     "sounds/pickup.ott" // Звук сбора бонуса
#define SOUND_POP_NAME        "sounds/pop.ott"    // Звук смерти/лопания мяча

// Физические константы
// Дискретизация движения: скорость делится на шаги для пошагового перемещения
// Значение 10 жестко закодировано в оригинальном Java Ball.java:980 и Ball.java:1136
// Обеспечивает баланс между плавностью движения и производительностью
#define MOVEMENT_STEP_DIVISOR 10

// Смещение для подталкивания застрявшего мяча при инициализации в тесном месте
// Соответствует byte b1 = 4 в оригинальном Java коде
#define STUCK_BALL_OFFSET 4

// Размеры мяча
#define BALL_SIZE_SMALL 0
#define BALL_SIZE_LARGE 1

// Состояния размера мяча
typedef enum {
    SMALL_SIZE_STATE = 0,    // Маленький мяч: 12px спрайт, обычная физика
    LARGE_SIZE_STATE = 1     // Большой мяч: 16px спрайт, увеличенная сила прыжка
} BallSizeState;

// Состояния мяча
typedef enum {
    BALL_STATE_NORMAL = 0,   // Обычное состояние: активная физика, реагирует на ввод
    BALL_STATE_DEAD = 1,     // Мяч уничтожен: нужно респавнить в стартовой точке
    BALL_STATE_POPPED = 2    // Мяч лопнул: временная анимация, затем DEAD
} BallState;

// Битовые флаги направления движения (из BounceConst.java)
typedef enum {
    MOVE_LEFT = 1,     // Движение влево
    MOVE_RIGHT = 2,    // Движение вправо
    MOVE_DOWN = 4,     // Движение вниз
    MOVE_UP = 8        // Прыжок/движение вверх
} MoveDirection;

// Маска направлений движения (битовая комбинация MoveDirection)
typedef uint8_t MoveMask;

// Размеры игрового поля
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 272

// Игровые ограничения
#define MAX_LEVEL 11            // Максимальный номер уровня
#define SCORE_DIGITS 8          // Количество цифр для форматирования счета

// Очки за игровые события (соответствуют BounceConst.java)
#define RING_POINTS   500       // Очки за сбор кольца
#define GEM_POINTS    200       // Очки за сбор драгоценного камня  
#define LIFE_POINTS   1000      // Очки за дополнительную жизнь
#define EXIT_POINTS   5000      // Очки за завершение уровня

// Константы для спрайтов
#define SPRITE_INDEX_INVALID 255

// Структура игрока (адаптированная из Ball.java)
// ПРИМЕЧАНИЕ: Все целочисленные поля используют int для точного соответствия Java,
// сохраняя идентичную целочисленную математику без риска сужений типов
typedef struct {
    // Позиция в экранных координатах (пиксели), центр мяча - как в Java (int)
    int xPos, yPos;
    
    // Глобальные координаты для точных коллизий (как в Ball.java)
    int globalBallX, globalBallY;
    
    // Скорость в единицах x0.1 пикселя/кадр для точности расчетов - как в Java (int)
    int xSpeed, ySpeed;
    
    // Битовые флаги направления движения (комбинация MoveDirection)
    MoveMask direction;
    
    // Размер спрайта мяча в пикселях (12 или 16)
    int ballSize;
    int mHalfBallSize;        // Половина размера для расчета коллизий
    
    // Смещение спрайта при прыжке для анимационного эффекта
    int jumpOffset;
    
    // Состояние мяча (BALL_STATE_*)
    BallState ballState;
    // Размер мяча (SMALL_SIZE_STATE или LARGE_SIZE_STATE)
    BallSizeState sizeState;
    
    // Флаги физического состояния
    bool mGroundedFlag;        // true если мяч касается земли/платформы
    bool mCDRubberFlag;        // true если коллизия с резиновой поверхностью
    
    // Счетчики временных эффектов (в кадрах) - как в Java (int)
    int speedBonusCntr;       // Остаток времени бонуса скорости
    int gravBonusCntr;        // Остаток времени бонуса гравитации  
    int jumpBonusCntr;        // Остаток времени бонуса прыжка
    
    // Счетчик анимации лопания мяча (5 кадров) - как в Java (int)
    int popCntr;              // Остаток времени анимации после pop_ball()
    
    // Счетчик скольжения по поверхности - как в Java (int)
    int slideCntr;
    
    // Флаг нахождения в воде (тайл с флагом TILE_FLAG_WATER)
    bool isInWater;
    
    // Флаг коллизии с рампой (для точной физики)
    bool mCDRampFlag;
} Player;

// Глобальное состояние игры
typedef struct {
    GameState state;          // Текущее состояние игры
    int menu_selection;       // Выбранный пункт меню
    int selected_level;       // Выбранный уровень (1-MAX_LEVEL)
    Player player;            // Состояние игрока
    
    // Игровая статистика (Java-совместимые поля)
    int numRings;             // Количество собранных колец
    int score;                // Очки игрока (500 за кольцо)
    int numLives;             // Количество жизней (начинается с 3, максимум 5)
    // Анимация двери теперь управляется из game.c
    
    // Отладочные флаги
    bool invincible_cheat;      // Читерское бессмертие (как mInvincible в Java)
    
    // Splash screen система
    int splash_timer;           // Таймер для splash экранов
    texture_t* nokia_splash_texture;  // Текстура Nokia Games splash
    texture_t* bounce_splash_texture; // Текстура Bounce splash
    
    // Saved game state для Continue
    SavedGameState saved_game_state; // Состояние сохраненной игры

    // Экран инструкций
    int instruction_part;       // Текущая отображаемая часть инструкций (0-5)

    // Флаг нового рекорда (как mNewBestScore в оригинале BounceUI.java:33)
    bool new_best_score;        // true если текущая игра установила новый рекорд

} Game;

extern Game g_game;

// Функции физики игрока (реализованы в отдельном файле физики)
void player_init(Player* p, int x, int y, BallSizeState sizeState);
void player_update(Player* p);
void set_direction(Player* p, MoveDirection dir);
void release_direction(Player* p, MoveDirection dir);
void enlarge_ball(Player* p);
void shrink_ball(Player* p);
void pop_ball(Player* p);

// Игровые события (callbacks)
void game_add_score(int points);
void game_add_ring(void);
void game_ring_collected(int tileX, int tileY, uint8_t tileID);

void game_set_respawn(int x, int y);
void game_add_extra_life(void);
void game_complete_level(void);

// === СИСТЕМА СОХРАНЕНИЙ ===
typedef struct {
    int best_level;    // Максимальный достигнутый уровень (1-11)
    int best_score;    // Лучший счёт
    int magic;         // Проверка валидности файла (0x424F554E = "BOUN")
} SaveData;

// Функции сохранений
void save_init(void);                              // Инициализация, загрузка данных
void save_shutdown(void);                          // Очистка ресурсов
void save_flush(void);                             // Принудительное сохранение
void save_update_records(int level, int score);    // Обновить рекорды если нужно
SaveData* save_get_data(void);                     // Получить текущие рекорды

// Utility functions
FILE* util_open_file(const char* path, const char* mode);


#ifdef __cplusplus
}
#endif

#endif
