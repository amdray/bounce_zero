# Сравнительный анализ Bounce: порт на C (PSP), оригинальная Java и Bounce Back

## Обзор проектов

| Аспект | bounce_zero/src (C порт) | bounce_zero/original_code (Java) | bounce_back/original_code (Java) |
|--------|--------------------------|----------------------------------|----------------------------------|
| **Платформа** | PSP (PSPSDK) | J2ME MIDP 1.0 (Nokia) | J2ME MIDP 1.0 (Nokia S60) |
| **Язык** | C | Java | Java (обфусцированный) |
| **Пакет** | — | com.nokia.mid.appl.boun | root (a.java, b.java, c.java...) |
| **Основная игра** | Bounce (первая часть) | Bounce (первая часть) | Bounce Back (вторая часть) |

---

## Файлы уровней

| Параметр | bounce_zero/src | bounce_zero/original_code | bounce_back/original_code | Рекомендации по модификации |
|----------|-----------------|---------------------------|---------------------------|---------------------------|
| **Путь** | `levels/J2MElvl.XXX` | `/levels/J2MElvl.XXX` | `/res/lf` (бинарный контейнер) | **КРИТИЧНО**: bounce_back использует контейнерный формат через класс `c.java` вместо отдельных файлов |
| **Формат файла** | Бинарный, прямая загрузка | Бинарный, DataInputStream | Бинарный контейнер с индексами | Реализовать парсер контейнера `/res/lf` по образцу `c.java` |
| **Количество уровней** | 11 (`MAX_LEVEL=11`) | 11 | 22 (включая tutorial на позиции 21) | Увеличить `MAX_LEVEL` |
| **Структура заголовка** | 8 байт: startX, startY, ballSize, exitX, exitY, totalRings, width, height | Идентично C порту | Отличается: b1(тема), b2(spawnY), b3(spawnX), b4(ballType), ar, D, j(enemies) | Полностью переделать `level_load_from_memory()` |
| **Размер тайла** | 12×12 px (`TILE_SIZE=12`) | 12×12 px | **16×16 px** (`this.f=16, this.A=16`) | **КРИТИЧНО**: все маски коллизий в `level_masks.inc` привязаны к 12px, потребуется регенерация |
| **Формат тайловой карты** | `short[height][width]`, прямое чтение | `short[height][width]`, DataInputStream | `byte[n][ac]` через контейнер `/res/lf` | Адаптировать парсер для нового формата |
| **Движущиеся объекты** | `MovingObject` структура после карты | `mMOTopLeft[][]`, `mMOBotRight[][]` | В заголовке уровня (j штук), формат отличается | Переписать парсер движущихся объектов |

---

## Файлы текстур

| Параметр | bounce_zero/src | bounce_zero/original_code | bounce_back/original_code | Рекомендации по модификации |
|----------|-----------------|---------------------------|---------------------------|---------------------------|
| **Основной атлас** | `icons/objects_nm.png` | Генерируется из `loadTileImages()` | `/res/if0`, `/res/if1`, `/res/if2` (по темам) | Добавить загрузку тематических атласов |
| **Формат атласа** | PNG, 12×12 px тайлы | runtime Image[], 12×12 px | Контейнерный бинарный формат, 16×16 px | Конвертировать `if*` или парсить напрямую |
| **Splash экраны** | `icons/nokiagames.png`, `icons/bouncesplash.png` (STATE_SPLASH_NOKIA, STATE_SPLASH) | `/icons/nokiagames.png`, `/icons/bouncesplash.png` (SPLASH_NAME array) | Нет splash экранов | Опционально, специфика первой части |
| **UI элементы** | Из атласа | `mUILife`, `mUIRing` из `loadTileImages()` | `/res/ic` контейнер (3 изображения) | Добавить загрузку из `/res/ic` |
| **Фоны уровней** | Цвет `BACKGROUND_COLOUR` | Цвет 11591920 (Java int) | `/res/bg` + `/res/ib0`, `/res/ib1` по темам | Реализовать фоновые слои |
| **Анимации мяча** | `ballSize` переменная, спрайты из атласа | 25 спрайтов загружаются динамически | `/res/b` - 25 спрайтов мяча (a.java:68-90) | Добавить загрузку анимаций из `/res/b` |
| **Темы визуала** | Одна тема | Одна тема | 3 темы (0,1,2) выбираются по уровню (b1 в заголовке) | Добавить систему тем |

---

## Файлы локализаций

| Параметр | bounce_zero/src | bounce_zero/original_code | bounce_back/original_code | Рекомендации по модификации |
|----------|-----------------|---------------------------|---------------------------|---------------------------|
| **Путь** | Загружается в runtime (код `local.c`) | `/lang.XX` где XX=locale | Хардкод в `CrystalMidlet.java` | bounce_back не имеет внешней локализации |
| **Формат** | Бинарный с UTF offsets | Бинарный с UTF offsets (DataInputStream) | Java String[] массивы | Перенести строки из Java в структуру данных |
| **Строки меню** | 21 строка (QHJ_*, QTJ_*) | 21 строка (Local.java) | ~30+ строк в `r[]`, `z[]`, `E[]`, `l[]`, `w[]`, `d[]`, `b[]` | Расширить структуру локализации |
| **Языки** | `lang.ru-RU`, `lang.de`, `lang.xx` и др. | 8+ языков | Только английский (хардкод) | Единственный язык, упрощает |
| **API** | `local_get_text(id)` | `Local.getText(id)` | Прямой доступ к массивам | Адаптировать под хардкод строки |

---

## Файлы звуков

| Параметр | bounce_zero/src | bounce_zero/original_code | bounce_back/original_code | Рекомендации по модификации |
|----------|-----------------|---------------------------|---------------------------|---------------------------|
| **Путь** | `sounds/up.ott`, `sounds/pickup.ott`, `sounds/pop.ott` | `/sounds/up.ott`, `/sounds/pickup.ott`, `/sounds/pop.ott` | `/res/s` контейнер | Добавить парсер `/res/s` |
| **Формат** | Nokia OTT (ринготоны) | Nokia OTT | Nokia OTT (внутри контейнера) | Логика та же, только извлечение другое |
| **Количество звуков** | 3 | 3 | Неизвестно (в контейнере) | Проанализировать `/res/s` |
| **API воспроизведения** | `sound_play_hoop()`, `sound_play_pickup()`, `sound_play_pop()` | `mSoundHoop.play(1)` и т.д. | `this.o.b(N)` где N - индекс звука | Расширить звуковую систему |
| **Парсинг OTT** | `sound.c` - полный парсер | com.nokia.mid.sound.Sound | Аналогично, через контейнер | Использовать существующий парсер |

---

## Соответствие Entity ↔ Текстура (анимация)

| Параметр | bounce_zero/src | bounce_zero/original_code | bounce_back/original_code | Рекомендации по модификации |
|----------|-----------------|---------------------------|---------------------------|---------------------------|
| **Таблица тайлов** | `tile_table.c` - статическая таблица `TileMeta` | `loadTileImages()` в TileCanvas.java, runtime | `/res/tf` - бинарный формат | **КРИТИЧНО**: tf содержит: collision type, sprite index, transform, animation data |
| **Индексы спрайтов** | `sprite_index` в TileMeta | Массив `tileImages[67]` | `v[]`, `T[]`, `b[]` в g.java | Переписать `tile_table.c` для нового формата |
| **Трансформации** | `TileTransform` enum (TF_NONE..TF_ROT_270_FLIP_XY) | `manipulateImage()` case 0-5 | `p[]` массив: {0,270,180,90,16384...} - комбинации | Расширить enum трансформаций |
| **Анимации тайлов** | Нет поддержки | Нет (статичные тайлы) | `m[][]`, `O[]`, `ai[]`, `aa[]` в g.java | **НОВОЕ**: добавить систему анимации тайлов |
| **Collision data** | Хардкод в `level_masks.inc` | Хардкод в Ball.java | `l[]`, `s[][][]` в g.java - per-tile collision masks | Загружать маски из `/res/tf` |

---

## Соответствие Entity ↔ Механика объекта

| Параметр | bounce_zero/src | bounce_zero/original_code | bounce_back/original_code | Рекомендации по модификации |
|----------|-----------------|---------------------------|---------------------------|---------------------------|
| **ID тайлов** | 0-54 (TILE_* константы) | 0-54 (ID_* в BounceConst) | 0-116+ (расширенный набор) | Расширить диапазон ID тайлов |
| **Шипы** | ID 3-6, 10 | ID 3-6, 10 | ID 1, 62-65, 85-92 | Добавить новые типы шипов |
| **Кольца** | ID 13-28 (active/inactive, size) | ID 13-28 | ID 93-102 (другая система!) | **КРИТИЧНО**: полностью другая нумерация колец |
| **Бонусы** | ID 38-54 (разные типы) | ID 38-54 | ID 11 (инверсия вкл), 15 (grav), 18 (инверсия выкл), 22 (jump), 26 (speed), 39 (speed2, a.java:489-530) | **КРИТИЧНО**: ID 11/18 переключают `this.I` boolean |
| **Collectibles** | — | — | ID 12, 30, 34 (коллекция очков, a.java:880-883) | **НОВОЕ**: добавить систему collectibles |
| **Слопы/рампы** | ID 30-37 | ID 30-37 | ID 3-6, 52, 113-116 | Расширить типы рамп |
| **Платформы** | ID 1, 2 | ID 1 (red), 2 (blue/rubber) | ID 2, 110, 111, 112 | Добавить новые платформы |
| **Switch функция** | `physics.c` большой switch | `testTile()` в Ball.java | `a.java:756-1632` (очень большой) | Расширить switch обработки |

---

## Меню игры и его структура

| Параметр | bounce_zero/src | bounce_zero/original_code | bounce_back/original_code | Рекомендации по модификации |
|----------|-----------------|---------------------------|---------------------------|---------------------------|
| **Файл** | `menu.c` | BounceUI.java | `i.java` + `CrystalMidlet.java` | Адаптировать структуру меню |
| **Состояния меню** | `GameState` enum (7 состояний) | SPLASH_NOKIA/GAME, displayMainMenu() и др. | `g` byte в i.java (0-12 типов) | Добавить новые состояния |
| **Пункты главного меню** | Continue, New Game, Select Level, High Score, Instructions | Аналогично | Continue, New Game, Options, Records, Help, Exit | Добавить Options |
| **Options меню** | Нет | Нет | Sounds On/Off, Vibra On/Off, Record On/Off | **НОВОЕ**: добавить Options экран |
| **Level Complete** | `STATE_LEVEL_COMPLETE` | `displayLevelComplete()` | Отдельный экран с Points/Bonus/Total | Добавить подсчет бонуса |
| **Game Over** | `STATE_GAME_OVER` | `displayGameOver()` | Levels Completed, Score, New Record | Расширить информацию |
| **Help/Tutorial** | Instructions (6 страниц) | 6 частей инструкций | Game Help + Controls + Tutorial уровень 21 | Добавить tutorial уровень |

---

## Таймеры игры

| Параметр | bounce_zero/src | bounce_zero/original_code | bounce_back/original_code | Рекомендации по модификации |
|----------|-----------------|---------------------------|---------------------------|---------------------------|
| **Основной таймер** | PSP vblank sync (~60 FPS) | 30ms interval (33 FPS) | **50ms interval (20 FPS)** | **КРИТИЧНО**: физика рассчитана на 20 FPS |
| **Константа** | Неявная (vblank) | `SPLASH_TIMER_DELAY = 30` | 50ms в f.java:12, h.java:811 | Добавить множитель времени или фиксировать 20 FPS |
| **API таймера** | `sceDisplayWaitVblankStart()` | `java.util.Timer` + TimerTask | `java.util.Timer` + TimerTask | Аналогично |
| **Game loop** | `game_update()`, `game_render()` | `run()` в TileCanvas | `run()` в h.java:752-807 | Структура похожа |
| **Delta time** | Неявный (фиксированный кадр) | Фиксированный | `this.g += System.currentTimeMillis() - this.t` | Добавить delta time tracking |
| **Бонус таймеры** | `speedBonusCntr`, `gravBonusCntr`, `jumpBonusCntr` (кадры) | Аналогично, 300 кадров | `this.B` = 450 кадров для всех бонусов (a.java:505,512,519) | Увеличить длительность бонусов (450 vs 300) |

---

## Размер тайла

| Параметр | bounce_zero/src | bounce_zero/original_code | bounce_back/original_code | Рекомендации по модификации |
|----------|-----------------|---------------------------|---------------------------|---------------------------|
| **Константа** | `TILE_SIZE = 12` (tile_table.h) | `TILE_SIZE = 12` (BounceConst) | `this.f = 16, this.A = 16` (g.java:162-165) | **КРИТИЧНО**: изменить на 16 |
| **Зависимости** | `level_masks.inc`, все коллизии | TRI_TILE_DATA, BALL_DATA | `s[][][]` - маски коллизий 16×16 | Регенерировать ВСЕ маски |
| **Проверка** | `_Static_assert(TILE_SIZE == 12)` | — | — | Убрать assert или изменить на 16 |
| **Spike size** | 24px (2 тайла) | `SPIKE_SIZE = 24` | ~32px (2 тайла по 16) | Пересчитать размеры |
| **Ball collision** | `SMALL_BALL_DATA[12][12]`, `LARGE_BALL_DATA[16][16]` | Идентично | Загружается из `/res/b` | Адаптировать под новые размеры |

---

## Состояние персонажа

| Параметр | bounce_zero/src | bounce_zero/original_code | bounce_back/original_code | Рекомендации по модификации |
|----------|-----------------|---------------------------|---------------------------|---------------------------|
| **Структура** | `Player` в types.h | `Ball` class | `a` class (обфусцировано) | Расширить структуру Player |
| **Состояния** | `BALL_STATE_NORMAL=0`, `_DEAD=1`, `_POPPED=2` | `ballState` 0/1/2 | Аналогично + `this.e` (dying flag) | Добавить dying анимацию |
| **Размер** | `SMALL_SIZE_STATE=0`, `LARGE_SIZE_STATE=1` + подводная плавучесть (UWATER_LARGE_MAX_GRAVITY=-30) | 0/1 + подводная плавучесть (UWATER_LARGE_MAX_GRAVITY=-30) | `this.t` boolean + `this.I` (постоянная инверсия) | Разные механизмы: zero - временная через бонус, back - постоянный флаг |
| **Инверсия гравитации** | `gravBonusCntr` (ID_GRAVITY_* тайлы 47-50) → 300 тиков инверсии, `gravity *= -1` | `gravBonusCntr = 300` (ID_GRAVITY_*) → 300 тиков инверсии | `this.I` boolean - постоянное состояние, переключается через специальный entity | **КРИТИЧНО**: zero - временный бонус (300 тиков), back - постоянное состояние |
| **Deflate/Inflate** | `shrink_ball()`, `enlarge_ball()` | Идентично | `this.a` для анимации, `this.q` для процесса | Добавить анимацию перехода |
| **Смерть** | `pop_ball()` → respawn | `popBall()` → respawn | `k()` → анимация → respawn | Добавить анимацию смерти |
| **Respawn** | `level_set_respawn()`, `level_get_respawn()` | `setRespawn()`, `respawnX/Y` | `this.H`, `this.n` (позиция в тайлах) | Аналогично |

---

## Константы физики

| Параметр | bounce_zero/src | bounce_zero/original_code | bounce_back/original_code | Рекомендации по модификации |
|----------|-----------------|---------------------------|---------------------------|---------------------------|
| **Файл** | `types.h`, `physics.c` | `BounceConst.java`, `Ball.java` | `a.java` (методы `h()`, `f()`, коллизии в `d()`) | Адаптировать под новую физику |
| **Прыжок нормальный** | `JUMP_STRENGTH = -67` | -67 | `-125` (a.java:543 метод h()) | Увеличить силу прыжка |
| **Прыжок инвертир.** | -67 (тот же) | -67 | `-180` (a.java:545) | **КРИТИЧНО**: инверсия требует +37% силы |
| **Прыжок popped** | — | — | `-95` (a.java:547) | **НОВОЕ**: лопнувший мяч слабее прыгает |
| **Прыжок бонус** | `JUMP_BONUS_STRENGTH = -80` | -80 | Модифицируется через `i += i >> 2` (+25%, a.java:549-555) | Другая формула расчета |
| **Горизонт. ускорен.** | `HORZ_ACCELL = 6` | 6 | 18 (a.java:1394), 22 при бонусе (a.java:1394) | **КРИТИЧНО**: в 3 раза быстрее |
| **Макс. горизонт.** | `MAX_HORZ_SPEED = 50` | 50 | 60 (a.java:1398), 100 при бонусе (a.java:1398) | Увеличить пределы в 2 раза |
| **Трение горизонт.** | `FRICTION_DECELL = 4` | 4 | 3 (a.java:1410 в воздухе), 8 на земле (a.java:1410) | Разделить на ground/air |

---

## Маппинг кнопок

| Параметр | bounce_zero/src | bounce_zero/original_code | bounce_back/original_code | Рекомендации по модификации |
|----------|-----------------|---------------------------|---------------------------|---------------------------|
| **Файл** | `input.c`, `game.c` | `BounceCanvas.keyPressed()` | `h.java:c()`, `h.java:keyPressed()` | Аналогичная структура |
| **Влево** | `PSP_CTRL_LEFT` → `MOVE_LEFT=1` | KEY_NUM4 | KEY_NUM4 | Совместимо |
| **Вправо** | `PSP_CTRL_RIGHT` → `MOVE_RIGHT=2` | KEY_NUM6 | KEY_NUM6 | Совместимо |
| **Прыжок** | `PSP_CTRL_CROSS` → `MOVE_UP=8` | KEY_NUM2, KEY_NUM5 | KEY_NUM2, KEY_NUM5 | Совместимо |
| **Меню/Пауза** | `PSP_CTRL_START` | Soft keys | Soft keys | Совместимо |
| **Накапливание** | `input_consume_pressed()` | `direction \|= dir` | `direction \|= dir` | Идентично |
| **Отпускание** | `input_consume_released()` | `direction &= ~dir` | `direction &= ~dir` | Идентично |

---

## Камера и экран

| Параметр | bounce_zero/src | bounce_zero/original_code | bounce_back/original_code | Рекомендации по модификации |
|----------|-----------------|---------------------------|---------------------------|---------------------------|
| **Файл** | `game.c` (camera logic) | `TileCanvas.java`, `screenFlip()` | `h.java:g()`, `h.java:a()` | Адаптировать логику камеры |
| **Размер экрана** | 480×272 (PSP native) | 128×128 (Nokia) | 176×208 (S60) | Масштабировать под PSP |
| **Игровая область** | 480×(272-17) = 480×255 | 128×96 (буфер) | 176×179 (g.java:121) | Адаптировать размеры |
| **HUD высота** | 17px | 31px (97-128) | Нижняя часть ~29px | Настроить HUD |
| **Буфер рендера** | Прямой рендер в VRAM | 156×96 Image buffer | Прямой рендер | Аналогично |
| **Тип скролла** | Deadzone вертикальный | Screen flip (7 тайлов) | Screen flip + smooth | Добавить smooth scrolling |
| **Horizontal scroll** | Deadzone центрированный | Left/Center/Right zones | Аналогично + плавный | Адаптировать логику |
| **Camera bounds** | Автоматические | `tileX`, `tileY`, `divisorLine` | `u`, `o`, `B`, `ah`, `ag`, `c` | Новые переменные камеры |

---

## Критические различия и приоритеты модификации

### 🔴 КРИТИЧНО (блокирующие изменения)

1. **Размер тайла 12→16 px**
   - Все collision masks в `level_masks.inc` требуют регенерации
   - `TILE_SIZE` constant и все зависимости
   - Spritesheets и атласы

2. **Формат контейнера ресурсов**
   - Реализовать парсер `c.java` для чтения `/res/*` файлов
   - `/res/lf` (уровни), `/res/tf` (тайлы), `/res/if*` (текстуры), `/res/b` (мяч), `/res/s` (звуки)

3. **Нумерация тайлов**
   - Кольца: 13-28 → 93-102
   - Шипы: 3-6 → 1, 62-65, 85-92
   - Требуется полная переработка tile_table.c

4. **Физика 33 FPS → 20 FPS**
   - Все константы физики рассчитаны на другой framerate
   - Добавить множитель времени или фиксировать 20 FPS

### 🟡 ВАЖНО (новая функциональность)

5. **Инвертированный мяч**
   - Новая механика: большой мяч всплывает в воде
   - Инвертированная гравитация
   - Специальные collectibles для инвертированного состояния

6. **Система тем**
   - 3 визуальные темы (0, 1, 2)
   - Разные текстуры фона и тайлов

7. **Анимации тайлов**
   - Система `m[][]`, `O[]`, `ai[]`, `aa[]` в g.java
   - Отсутствует в оригинальном Bounce

8. **Tutorial уровень**
   - Уровень 21 с текстовыми подсказками
   - Система `r[]` строк

### 🟢 ЖЕЛАТЕЛЬНО (улучшения)

9. **Options меню**
   - Sounds On/Off
   - Vibration On/Off

10. **Расширенный подсчет очков**
    - Points + Bonus + Total на Level Complete
    - Time-based bonus calculation

---

## Пример структуры данных `/res/lf` (h.java:195-302)

```c
// Заголовок уровня Bounce Back
typedef struct {
    uint8_t theme_id;      // b1 - индекс темы (0, 1, 2)
    uint8_t spawn_y;       // b2 - Y координата спавна в тайлах
    uint8_t spawn_x;       // b3 - X координата спавна в тайлах  
    uint8_t ball_type;     // b4 - 0=маленький, другое=большой
    uint8_t ar;            // ar - доп. параметр
    uint8_t D;             // D - доп. параметр
    uint8_t enemy_count;   // j - количество врагов
    // Далее enemy_count записей по 9 байт каждая
} BounceBackLevelHeader;

// Враг/движущийся объект
typedef struct {
    uint8_t type;          // тип объекта
    uint8_t x1, y1;        // начальная позиция
    uint8_t x2, y2;        // конечная позиция
    uint8_t offset_x;      // смещение X (умножить на 16)
    uint8_t offset_y;      // смещение Y (умножить на 16)
    int8_t speed_x;        // скорость X
    int8_t speed_y;        // скорость Y
} BounceBackEnemy;
```

---

## Пример парсера контейнера (по образцу c.java)

```c
// Контейнер ресурсов Bounce Back
typedef struct {
    int count;              // количество элементов
    int* offsets;           // смещения до элементов
    short* sizes;           // размеры элементов
    uint8_t* data;          // данные всего файла
} ResourceContainer;

// Загрузка контейнера (аналог c.java конструктора)
ResourceContainer* container_load(const char* path) {
    FILE* f = fopen(path, "rb");
    // Читаем count как big-endian short
    int count = (fgetc(f) << 8) | fgetc(f);
    // Выделяем массивы
    int* offsets = malloc(count * sizeof(int));
    short* sizes = malloc(count * sizeof(short));
    // Читаем таблицу размеров
    int current_offset = (count + 1) * 2;
    for (int i = 0; i < count; i++) {
        offsets[i] = current_offset;
        sizes[i] = (fgetc(f) << 8) | fgetc(f);
        current_offset += sizes[i];
    }
    // Читаем данные...
}

// Получение элемента по индексу (аналог c.java.a())
uint8_t* container_get(ResourceContainer* c, int index, int* out_size);
```
