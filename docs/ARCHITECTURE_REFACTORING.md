# Архитектурный план модульной переработки Bounce Zero

**Дата создания:** 2 февраля 2026  
**Версия:** 1.0  
**Цель:** Изоляция PSP SDK зависимостей в драйверном слое для обеспечения портируемости и модульности кода

---

## 1. Анализ PSP-специфичных узлов

### 1.1 Полная картина зависимостей от PSP SDK

**Всего обнаружено:** 90+ вызовов `sce*` функций и 31 `psp*` функций в 11 файлах исходного кода

### 1.2 Классификация по зонам ответственности

#### **Графическая подсистема (GU — Graphics Unit)** [graphics.c: 52 вызова]

| Функция/Группа | Частота | Назначение | Критичность |
|----------------|---------|------------|-------------|
| `sceGuStart/sceGuFinish/sceGuSync` | 6 | Display List управление (начало/конец/синхронизация) | ⚠️ Критично |
| `sceGuDrawArray` | 3 | Отрисовка примитивов (SPRITES, TRIANGLE_STRIP) | ⚠️ Критично |
| `sceGuGetMemory` | 3 | Динамическая аллокация вершин из Display List | ⚠️ Критично |
| `sceGuSwapBuffers` | 1 | Переключение буферов (double buffering) | ⚠️ Критично |
| `sceGuInit/sceGuTerm/sceGuDisplay` | 3 | Инициализация/завершение/вкл-выкл дисплея | ⚠️ Критично |
| `sceGuDrawBuffer/sceGuDispBuffer` | 2 | Настройка VRAM framebuffers | ⚠️ Критично |
| `sceGuViewport/sceGuOffset/sceGuScissor` | 5 | Viewport и clipping области | Высокая |
| `sceGuEnable/sceGuDisable` | 5 | Переключение состояний (BLEND, TEXTURE_2D, ALPHA_TEST, SCISSOR) | Высокая |
| `sceGuBlendFunc` | 1 | Настройка альфа-блендинга | Средняя |
| `sceGuColor` | 1 | Установка цвета примитива | Средняя |
| `sceGuClear/sceGuClearColor` | 4 | Очистка framebuffer | Средняя |
| `sceGuClutMode/sceGuClutLoad` | 3 | Палитры для индексированных текстур (T4 шрифты) | Средняя |
| `sceGuTexImage/sceGuTexMode/sceGuTexFunc` | 6 | Загрузка и настройка текстур | Средняя |
| `sceGuTexFilter/sceGuTexWrap` | 2 | Фильтрация и wrapping текстур | Низкая |
| `sceGuAlphaFunc` | 1 | Alpha test функция | Низкая |

**Особенности:**
- Display Lists (256KB буфер команд) — центральный элемент архитектуры GU
- Батчинг спрайтов (`MAX_SPRITES_PER_BATCH = 128`) для минимизации вызовов
- VRAM буферы с выравниванием по степени двойки (512×272)
- GU_SPRITES использует 2 вершины на спрайт (top-left, bottom-right)

#### **Дисплей и VSync (Display Controller)** [graphics.c, save.c: 3 вызова]

| Функция | Частота | Файлы | Назначение |
|---------|---------|-------|------------|
| `sceDisplayWaitVblankStart` | 3 | graphics.c, save.c | Синхронизация с вертикальной развёрткой (60 FPS lock) |
| `sceGuDisplay` | 2 | graphics.c | Включение/выключение вывода на экран (GU_TRUE/GU_FALSE) |

#### **Ввод (Control Pad)** [input.c: 3 вызова]

| Функция | Частота | Назначение | Контекст |
|---------|---------|------------|----------|
| `sceCtrlSetSamplingCycle` | 1 | Установка частоты опроса (0 = максимальная) | Инициализация |
| `sceCtrlSetSamplingMode` | 1 | Режим чтения контроллера (PSP_CTRL_MODE_DIGITAL) | Инициализация |
| `sceCtrlReadBufferPositive` | 1/кадр | Чтение состояния кнопок в SceCtrlData | Основной цикл |

**Структура данных:**
```c
// PSP-специфичная структура (используется ТОЛЬКО в input.c)
typedef struct SceCtrlData {
    unsigned int TimeStamp;
    unsigned int Buttons;  // Битовая маска (PSP_CTRL_UP, PSP_CTRL_CROSS, etc.)
    unsigned char Lx, Ly;  // Аналоговый стик (НЕ используется в Bounce)
    unsigned char Rsrv[6];
} SceCtrlData;
```

**Примечание:** Игра использует ТОЛЬКО цифровые кнопки (PSP_CTRL_MODE_DIGITAL), аналоговый стик игнорируется по дизайну оригинальной Java версии.

#### **Системные функции (Kernel)** [main.c: 10 вызовов, sound.c: 2 вызова]

| Функция | Частота | Файлы | Назначение | Критичность |
|---------|---------|-------|------------|-------------|
| `sceKernelGetSystemTimeWide` | 2/кадр | main.c | Высокоточный таймер (мкс) для физического тика 30ms | ⚠️ Критично |
| `sceKernelCreateCallback` | 1 | main.c | Создание callback для HOME-кнопки | ⚠️ Критично |
| `sceKernelRegisterExitCallback` | 1 | main.c | Регистрация callback выхода | ⚠️ Критично |
| `sceKernelCreateThread` | 1 | main.c | Создание callback-потока | ⚠️ Критично |
| `sceKernelStartThread` | 1 | main.c | Запуск callback-потока | ⚠️ Критично |
| `sceKernelSleepThreadCB` | 1 | main.c | Блокировка callback-потока | ⚠️ Критично |
| `sceKernelExitGame` | 2 | main.c | Завершение приложения | ⚠️ Критично |
| `sceKernelDcacheWritebackRange` | 4 | graphics.c, png.c, font_atlas.c | Сброс data cache (VRAM safety) | ⚠️ КРИТИЧНО для корректности |
| `sceKernelCpuSuspendIntr` | 1 | sound.c | Атомарность изменений аудио (отключение прерываний) | Высокая |
| `sceKernelCpuResumeIntr` | 1 | sound.c | Включение прерываний | Высокая |

**Архитектура callback-системы PSP:**
```c
// Callback-поток (приоритет 0x11, стек 0xFA0)
int main_callback_thread(SceSize args, void *argp) {
    int cbid = sceKernelCreateCallback("Exit Callback", main_exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();  // Блокируется до события (HOME-кнопка)
    return 0;
}

// Callback функция - вызывается системой при нажатии HOME
int main_exit_callback(int arg1, int arg2, void *common) {
    g_game.state = STATE_EXIT;  // Элегантное завершение через игровой цикл
    return 0;
}
```

#### **Аудио (Audio Library)** [sound.c: 6 вызовов]

| Функция | Частота | Назначение | Контекст |
|---------|---------|------------|----------|
| `pspAudioInit` | 1 | Инициализация PSP audio подсистемы | Возвращает < 0 при ошибке |
| `pspAudioSetChannelCallback` | 2 | Установка/сброс callback для канала 0 (44.1 kHz stereo) | Микшер 3 звуков |
| `pspAudioSetVolume` | 2 | Установка громкости (0x0000-0x8000) | 0x6000 = ~75% |
| `pspAudioEnd` | 1 | Завершение работы audio подсистемы | При shutdown |

**Архитектура аудио-микшера:**
```c
// Callback-driven аудио (вызывается системой ~60 раз/сек)
void ott_audio_callback(void* buf, unsigned int length, void *userdata) {
    psp_sample_t *samples = (psp_sample_t *)buf;  // Stereo interleaved
    for (unsigned int i = 0; i < length; i++) {
        // Генерация от 3 плееров (hoop, pickup, pop)
        short s0 = generate_player_sample(&g_hoop_player, sample_length);
        short s1 = generate_player_sample(&g_pickup_player, sample_length);
        short s2 = generate_player_sample(&g_pop_player, sample_length);
        
        int mix = (s0 + s1 + s2) / active_channels;  // Нормализация
        samples[i].l = samples[i].r = (short)clamp(mix, -32768, 32767);
    }
}
```

**Особенности:**
- Wavetable синтез (1024 сэмпла синусоиды, предвычисленная таблица)
- Fixed-point фаза (Q32 формат) для точности частоты
- Огибающая (attack/release Q15) для устранения щелчков
- OTT формат (Nokia ringtone format) — 44.1 kHz рендеринг в реальном времени

#### **Управление памятью (GE/VRAM)** [png.c: 2 вызова]

| Функция | Частота | Назначение | Критичность |
|---------|---------|------------|-------------|
| `sceGeEdramGetAddr` | 1 | Получение базового адреса VRAM (~0x04000000) | ⚠️ Критично |
| `sceGeEdramGetSize` | 1 | Получение размера VRAM (~2 MiB) | ⚠️ Критично |

**VRAM аллокатор (простая bump-allocation схема):**
```c
// Статический указатель (начинаем после framebuffers)
static unsigned int staticVramOffset = (VRAM_BUFFER_WIDTH * VRAM_BUFFER_HEIGHT * 4) * 2;

void* getStaticVramTexture(unsigned int width, unsigned int height, unsigned int psm) {
    unsigned int memSize = getTextureMemorySize(width, height, psm);
    if (staticVramOffset + memSize > sceGeEdramGetSize()) {
        return NULL; // VRAM переполнена — fallback в RAM
    }
    
    staticVramOffset = (staticVramOffset + 15) & ~15; // Выравнивание 16 байт
    void* result = (void*)(staticVramOffset + sceGeEdramGetAddr());
    staticVramOffset += memSize;
    return result;
}
```

**Критичные детали:**
- VRAM текстуры: адреса относительно `sceGeEdramGetAddr()`
- Framebuffers: смещения относительно 0 (не абсолютные адреса!)
- RAM текстуры: fallback при переполнении VRAM, требуют `sceKernelDcacheWritebackRange()`

#### **Система сохранений (Utility)** [save.c: 7 вызовов]

| Функция | Частота | Назначение |
|---------|---------|------------|
| `sceUtilityGetSystemParamInt` | 2 | Получение языка системы (PSP_SYSTEMPARAM_ID_INT_LANGUAGE) |
| `sceUtilitySavedataInitStart` | 1/операция | Инициализация диалога сохранения |
| `sceUtilitySavedataGetStatus` | N/кадр | Опрос статуса диалога (INIT, VISIBLE, QUIT) |
| `sceUtilitySavedataUpdate` | N/кадр | Обновление UI диалога |
| `sceUtilitySavedataShutdownStart` | 1/операция | Завершение диалога |

**Режимы работы:**
- `SCE_UTILITY_SAVEDATA_READDATA` — чтение существующего сохранения
- `SCE_UTILITY_SAVEDATA_WRITEDATA` — перезапись сохранения
- `SCE_UTILITY_SAVEDATA_MAKEDATA` — первичное создание слота

**Структура данных (16 байт):**
```c
typedef struct {
    int magic;        // 0x424F554E ("BOUN")
    int best_level;   // Максимальный открытый уровень (1-11)
    int best_score;   // Лучший счёт
    // 4 байта резерв
} SaveData;
```

#### **Локализация (Utility)** [local.c: 1 вызов]

| Функция | Назначение |
|---------|------------|
| `sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE)` | Определение языка PSP для выбора файла `lang/lang.xx` |

**Поддерживаемые языки:**
- English (xx), German (de), Russian (ru-RU), Danish (da-DK), Finnish (fi-FI), Icelandic (is-IS), Norwegian (no-NO), Swedish (sv)

#### **Файловая система (I/O)** 

| Функция | Файлы | Примечание |
|---------|-------|------------|
| `fopen/fread/fclose/fseek/ftell` | level.c, save.c, png.c, sound.c, local.c | Стандартная C библиотека (POSIX через newlib) |

**Примечание:** PSP SDK использует стандартные POSIX I/O функции, поэтому файловые операции **НЕ требуют абстракции** — работают идентично на других платформах.

---

### 1.3 Сводная таблица PSP SDK зависимостей

| Подсистема | Количество вызовов | Затронутые файлы | Критичность для портирования |
|------------|-------------------|------------------|-------------------------------|
| **Graphics (GU)** | 52 | graphics.c, png.c | ⚠️ МАКСИМАЛЬНАЯ |
| **Display (VSync)** | 3 | graphics.c, save.c | Высокая |
| **Input (Ctrl)** | 3 | input.c | Высокая |
| **Kernel (Threads/Time)** | 12 | main.c, sound.c | ⚠️ МАКСИМАЛЬНАЯ |
| **Memory (VRAM)** | 5 | graphics.c, png.c, font_atlas.c, level.c | ⚠️ МАКСИМАЛЬНАЯ |
| **Audio** | 6 | sound.c | Средняя |
| **Utility (Save/Lang)** | 8 | save.c, local.c | Средняя |
| **File I/O** | 0 (стандартный POSIX) | level.c, save.c, png.c, sound.c, local.c | ✅ Не требует портирования |
| **ИТОГО** | **89** вызовов | **11** файлов | - |

**Выводы:**
1. **Графика (GU)** — самая сложная подсистема (52 вызова), требует полной абстракции Display Lists
2. **Память (VRAM)** — критична для производительности, требует платформенного аллокатора
3. **Таймеры (Kernel)** — центральная роль в игровом цикле (физический тик 30ms)
4. **Файловый I/O** — полностью портируемый, обёртка не нужна

---

## 2. Проектирование "Драйверного слоя"

### 2.1 Архитектура изоляции

```
┌─────────────────────────────────────────┐
│      Игровая логика (game.c, etc.)      │
│   НЕ ЗНАЕТ О PSP SDK ЗАГОЛОВКАХ         │
└──────────────────┬──────────────────────┘
                   │
┌──────────────────▼──────────────────────┐
│   Абстрактные интерфейсы (HAL)          │
│   - video_device.h                      │
│   - input_system.h                      │
│   - platform_kernel.h                   │
│   - audio_mixer.h                       │
└──────────────────┬──────────────────────┘
                   │
┌──────────────────▼──────────────────────┐
│   Платформенный слой                    │
│   platforms/psp/                        │
│   - psp_video.c    (sceGu*)            │
│   - psp_input.c    (sceCtrl*)          │
│   - psp_kernel.c   (sceKernel*)        │
│   - psp_audio.c    (pspAudio*)         │
└─────────────────────────────────────────┘
```

---

## 3. Спецификация модулей

### 3.1 Модуль `video_device.h` — Абстракция графики

#### 3.1.1 Типы и константы

```c
#ifndef VIDEO_DEVICE_H
#define VIDEO_DEVICE_H

#include <stdint.h>
#include <stdbool.h>

// Платформенно-независимые константы
#define SCREEN_WIDTH  480
#define SCREEN_HEIGHT 272

// Форматы пикселей
typedef enum {
    PIXEL_FORMAT_RGBA8888,
    PIXEL_FORMAT_RGB565,
    PIXEL_FORMAT_INDEXED_4BIT  // Для T4 шрифтов
} PixelFormat;

// Режимы блендинга
typedef enum {
    BLEND_MODE_NONE,
    BLEND_MODE_ALPHA,      // src_alpha, 1-src_alpha
    BLEND_MODE_ADDITIVE    // src_alpha, one
} BlendMode;

// Контекст видео-устройства (opaque pointer для инкапсуляции)
typedef struct VideoDeviceContext VideoDeviceContext;

// Вершина для 2D примитивов
typedef struct {
    float x, y, z;
    uint32_t color;       // RGBA формат (порядок не специфичен для API)
    float u, v;           // Текстурные координаты
} Vertex2D;

// Дескриптор текстуры
typedef struct {
    void* gpu_data;       // Указатель на данные в видеопамяти (может быть offset или абсолютный адрес)
    int width, height;
    int stride;           // Фактическая ширина буфера (для выравнивания)
    PixelFormat format;
    bool owns_memory;     // Управляет ли структура памятью
} VideoTexture;
```

#### 3.1.2 Инициализация и управление

```c
/**
 * Инициализация видео-устройства
 * @param ctx Указатель на контекст (аллоцируется внутри функции)
 * @return 0 при успехе, < 0 при ошибке
 */
int video_device_init(VideoDeviceContext** ctx);

/**
 * Завершение работы видео-устройства
 */
void video_device_shutdown(VideoDeviceContext* ctx);

/**
 * Начать новый кадр (сброс command list)
 */
void video_device_begin_frame(VideoDeviceContext* ctx);

/**
 * Завершить кадр и выполнить swap buffers
 * @param vsync Ожидать вертикальную синхронизацию
 */
void video_device_end_frame(VideoDeviceContext* ctx, bool vsync);
```

#### 3.1.3 Управление состоянием

```c
/**
 * Установить область отсечения (scissor test)
 */
void video_device_set_scissor(VideoDeviceContext* ctx, int x, int y, int w, int h);

/**
 * Очистить framebuffer
 * @param color Цвет в формате 0xAABBGGRR (платформенно-независимый)
 */
void video_device_clear(VideoDeviceContext* ctx, uint32_t color);

/**
 * Установить режим блендинга
 */
void video_device_set_blend_mode(VideoDeviceContext* ctx, BlendMode mode);
```

#### 3.1.4 Отрисовка примитивов

```c
/**
 * Режимы рисования
 */
typedef enum {
    DRAW_MODE_SPRITES,   // Пары вершин (x0,y0)-(x1,y1)
    DRAW_MODE_TRIANGLES,
    DRAW_MODE_LINES
} DrawMode;

/**
 * Отрисовать массив вершин
 * @param vertices Массив вершин
 * @param count Количество вершин
 * @param texture Текстура (NULL для нетекстурированных примитивов)
 * @param mode Режим интерпретации вершин
 */
void video_device_draw_array(
    VideoDeviceContext* ctx,
    const Vertex2D* vertices,
    int count,
    VideoTexture* texture,
    DrawMode mode
);
```

#### 3.1.5 Управление текстурами

```c
/**
 * Создать текстуру из данных в RAM
 * @param data Указатель на пиксельные данные
 * @param width Логическая ширина
 * @param height Высота
 * @param stride Физическая ширина (для PSP: степень двойки)
 * @param format Формат пикселей
 * @return Дескриптор текстуры (NULL при ошибке)
 */
VideoTexture* video_device_create_texture(
    VideoDeviceContext* ctx,
    const void* data,
    int width,
    int height,
    int stride,
    PixelFormat format
);

/**
 * Установить палитру для индексированных текстур
 * @param colors Массив RGBA цветов
 * @param count Количество цветов
 */
void video_device_set_palette(VideoDeviceContext* ctx, const uint32_t* colors, int count);

/**
 * Освободить текстуру
 */
void video_device_free_texture(VideoDeviceContext* ctx, VideoTexture* tex);

/**
 * Синхронизировать изменения в видеопамяти (для PSP: dcache writeback)
 */
void video_device_sync_memory(VideoDeviceContext* ctx, void* addr, size_t size);
```

#### 3.1.6 Выделение буферов команд

```c
/**
 * Получить временную память для вершин (аналог sceGuGetMemory)
 * ВНИМАНИЕ: данные валидны только до следующего begin_frame()
 * @param size Размер в байтах
 * @return Указатель на буфер (NULL при переполнении)
 */
void* video_device_alloc_vertices(VideoDeviceContext* ctx, size_t size);
```

**Обоснование дизайна:**
- Функция `alloc_vertices()` заменяет `sceGuGetMemory()` для динамической генерации вершин
- На PSP данные аллоцируются из Display List, на других платформах — из заранее выделенного буфера
- Производительность: без `malloc()` на каждый примитив

---

### 3.2 Модуль `input_system.h` — Абстракция ввода

#### 3.2.1 Независимая структура состояний

```c
#ifndef INPUT_SYSTEM_H
#define INPUT_SYSTEM_H

#include <stdbool.h>
#include <stdint.h>

// Виртуальные кнопки (не привязаны к PSP_CTRL_*)
typedef enum {
    INPUT_BUTTON_UP     = 1 << 0,
    INPUT_BUTTON_DOWN   = 1 << 1,
    INPUT_BUTTON_LEFT   = 1 << 2,
    INPUT_BUTTON_RIGHT  = 1 << 3,
    INPUT_BUTTON_CROSS  = 1 << 4,  // Действие (Jump в Bounce)
    INPUT_BUTTON_CIRCLE = 1 << 5,  // Отмена
    INPUT_BUTTON_START  = 1 << 6,  // Пауза
    INPUT_BUTTON_SELECT = 1 << 7,  // Дополнительное меню
    INPUT_BUTTON_L      = 1 << 8,
    INPUT_BUTTON_R      = 1 << 9
} InputButton;

// Состояние ввода
typedef struct {
    uint32_t current;     // Текущие удерживаемые кнопки
    uint32_t pressed;     // Нажатые на этом кадре
    uint32_t released;    // Отпущенные на этом кадре
    uint32_t accumulated_pressed;  // Для фиксированного тика физики
    uint32_t accumulated_released;
    uint32_t lock_mask;   // Блокировка input_held() для навигации меню
} InputState;

// Контекст подсистемы ввода (opaque)
typedef struct InputContext InputContext;
```

#### 3.2.2 API функций

```c
/**
 * Инициализация подсистемы ввода
 */
int input_system_init(InputContext** ctx);

/**
 * Опрос контроллера (вызывать каждый кадр)
 * @param state Указатель на структуру состояния (заполняется функцией)
 */
void input_system_update(InputContext* ctx, InputState* state);

/**
 * Проверка нажатия (edge-triggered)
 */
static inline bool input_is_pressed(const InputState* state, InputButton button) {
    return (state->pressed & button) && !(state->lock_mask & button);
}

/**
 * Проверка удержания
 */
static inline bool input_is_held(const InputState* state, InputButton button) {
    return (state->current & button) && !(state->lock_mask & button);
}

/**
 * Проверка отпускания
 */
static inline bool input_is_released(const InputState* state, InputButton button) {
    return (state->released & button);
}

/**
 * Потребление накопленного нажатия (для фиксированного тика)
 */
bool input_consume_pressed(InputState* state, InputButton button);

/**
 * Сброс edge-состояний (при смене игрового состояния)
 */
void input_reset_edges(InputState* state);

/**
 * Блокировка удержания текущих кнопок (для меню)
 */
void input_lock_held(InputState* state);

/**
 * Завершение работы
 */
void input_system_shutdown(InputContext* ctx);

#endif // INPUT_SYSTEM_H
```

**Маппинг на PSP (platforms/psp/psp_input.c):**
```c
// Пример маппинга в платформенном слое
static const struct {
    InputButton generic;
    unsigned int psp_mask;
} button_mapping[] = {
    { INPUT_BUTTON_UP,    PSP_CTRL_UP },
    { INPUT_BUTTON_DOWN,  PSP_CTRL_DOWN },
    { INPUT_BUTTON_LEFT,  PSP_CTRL_LEFT },
    { INPUT_BUTTON_RIGHT, PSP_CTRL_RIGHT },
    { INPUT_BUTTON_CROSS, PSP_CTRL_CROSS },
    // ...
};
```

---

### 3.3 Модуль `platform_kernel.h` — Системные функции

```c
#ifndef PLATFORM_KERNEL_H
#define PLATFORM_KERNEL_H

#include <stdint.h>
#include <stdbool.h>

// Callback для выхода из приложения
typedef void (*ExitCallbackFunc)(void* userdata);

// Контекст платформы (opaque)
typedef struct PlatformContext PlatformContext;

/**
 * Инициализация платформенного слоя
 * @param exit_callback Функция обратного вызова для HOME-кнопки
 * @param userdata Пользовательские данные для callback
 */
int platform_init(PlatformContext** ctx, ExitCallbackFunc exit_callback, void* userdata);

/**
 * Получить текущее время в микросекундах
 */
uint64_t platform_get_time_us(PlatformContext* ctx);

/**
 * Получить текущее время в миллисекундах
 */
static inline uint64_t platform_get_time_ms(PlatformContext* ctx) {
    return platform_get_time_us(ctx) / 1000ULL;
}

/**
 * Завершение приложения
 * @param exit_code Код возврата
 */
void platform_exit(PlatformContext* ctx, int exit_code);

/**
 * Синхронизация данных в кэше (критично для DMA и GPU)
 * @param addr Начало области
 * @param size Размер в байтах
 */
void platform_dcache_writeback(PlatformContext* ctx, void* addr, size_t size);

/**
 * Отключение прерываний (для атомарных операций)
 * @return Токен для восстановления состояния
 */
int platform_suspend_interrupts(PlatformContext* ctx);

/**
 * Включение прерываний
 * @param token Токен от suspend_interrupts()
 */
void platform_resume_interrupts(PlatformContext* ctx, int token);

/**
 * Завершение работы
 */
void platform_shutdown(PlatformContext* ctx);

#endif // PLATFORM_KERNEL_H
```

**PSP реализация (platforms/psp/psp_kernel.c):**
```c
#include "platform_kernel.h"
#include <pspkernel.h>

struct PlatformContext {
    ExitCallbackFunc exit_callback;
    void* userdata;
};

// Внутренний callback для PSP SDK
static int psp_exit_callback(int arg1, int arg2, void* common) {
    PlatformContext* ctx = (PlatformContext*)common;
    if (ctx->exit_callback) {
        ctx->exit_callback(ctx->userdata);
    }
    return 0;
}

int platform_init(PlatformContext** ctx, ExitCallbackFunc exit_callback, void* userdata) {
    *ctx = malloc(sizeof(PlatformContext));
    (*ctx)->exit_callback = exit_callback;
    (*ctx)->userdata = userdata;
    
    // Создание callback-потока
    int cbid = sceKernelCreateCallback("Exit", psp_exit_callback, *ctx);
    sceKernelRegisterExitCallback(cbid);
    
    int thid = sceKernelCreateThread("callback_thread", ..., 0x11, 0xFA0, 0, 0);
    sceKernelStartThread(thid, 0, 0);
    
    return 0;
}

uint64_t platform_get_time_us(PlatformContext* ctx) {
    (void)ctx;
    return sceKernelGetSystemTimeWide();
}

void platform_dcache_writeback(PlatformContext* ctx, void* addr, size_t size) {
    (void)ctx;
    sceKernelDcacheWritebackRange(addr, size);
}

// ... остальные функции
```

---

### 3.4 Модуль `audio_mixer.h` — Абстракция аудио

```c
#ifndef AUDIO_MIXER_H
#define AUDIO_MIXER_H

#include <stdint.h>
#include <stdbool.h>

#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_MAX_CHANNELS 8

// Формат PCM данных
typedef enum {
    AUDIO_FORMAT_S16,  // Signed 16-bit
    AUDIO_FORMAT_F32   // Float 32-bit
} AudioFormat;

// Дескриптор звукового эффекта
typedef struct AudioClip AudioClip;

// Callback для генерации аудио
typedef void (*AudioMixerCallback)(void* userdata, int16_t* buffer, int frame_count);

// Контекст микшера (opaque)
typedef struct AudioMixerContext AudioMixerContext;

/**
 * Инициализация аудио-микшера
 * @param callback Функция обратного вызова для генерации аудио
 * @param userdata Пользовательские данные
 */
int audio_mixer_init(AudioMixerContext** ctx, AudioMixerCallback callback, void* userdata);

/**
 * Загрузить звук из OTT формата (Bounce-специфичный формат)
 * @param data Указатель на данные OTT
 * @param size Размер данных
 * @return Дескриптор клипа (NULL при ошибке)
 */
AudioClip* audio_mixer_load_clip(AudioMixerContext* ctx, const void* data, size_t size);

/**
 * Воспроизвести звук
 * @param clip Дескриптор клипа
 * @param volume Громкость (0.0 - 1.0)
 * @param loop Зацикливать ли воспроизведение
 * @return ID канала (< 0 при ошибке)
 */
int audio_mixer_play(AudioMixerContext* ctx, AudioClip* clip, float volume, bool loop);

/**
 * Остановить канал
 */
void audio_mixer_stop(AudioMixerContext* ctx, int channel_id);

/**
 * Освободить клип
 */
void audio_mixer_free_clip(AudioMixerContext* ctx, AudioClip* clip);

/**
 * Завершение работы
 */
void audio_mixer_shutdown(AudioMixerContext* ctx);

#endif // AUDIO_MIXER_H
```

---

## 4. Стратегия управления памятью

### 4.1 Проблема выравнивания и VRAM на PSP

PSP требует специфичного управления памятью:
- **VRAM аллокация:** Функции `vramalloc()` / `vramfree()` (не стандартные для C)
- **Выравнивание:** Текстуры должны иметь stride кратный степени двойки
- **Data cache:** Обязательный writeback перед DMA/GPU операциями

### 4.2 Абстракция через `platform_memory.h`

```c
#ifndef PLATFORM_MEMORY_H
#define PLATFORM_MEMORY_H

#include <stddef.h>
#include <stdbool.h>

typedef enum {
    MEM_TYPE_SYSTEM,    // Системная RAM (malloc)
    MEM_TYPE_VIDEO      // Видеопамять (на PSP: VRAM)
} MemoryType;

/**
 * Платформенная аллокация памяти
 * @param size Размер в байтах
 * @param alignment Выравнивание (степень двойки: 16, 64, и т.д.)
 * @param type Тип памяти
 * @return Указатель на выделенную память (NULL при ошибке)
 */
void* platform_mem_alloc(size_t size, size_t alignment, MemoryType type);

/**
 * Освобождение платформенной памяти
 */
void platform_mem_free(void* ptr, MemoryType type);

/**
 * Проверка: находится ли адрес в видеопамяти
 */
bool platform_mem_is_vram(const void* ptr);

#endif // PLATFORM_MEMORY_H
```

**PSP реализация (platforms/psp/psp_memory.c):**
```c
#include "platform_memory.h"
#include <pspge.h>
#include <stdlib.h>

// VRAM аллокатор PSP (простая версия)
static unsigned char* vram_base = (unsigned char*)0x04000000; // Начало VRAM
static unsigned char* vram_ptr = vram_base;

void* platform_mem_alloc(size_t size, size_t alignment, MemoryType type) {
    if (type == MEM_TYPE_VIDEO) {
        // Выравнивание VRAM указателя
        unsigned int addr = (unsigned int)vram_ptr;
        unsigned int aligned_addr = (addr + alignment - 1) & ~(alignment - 1);
        vram_ptr = (unsigned char*)aligned_addr;
        
        void* result = vram_ptr;
        vram_ptr += size;
        
        // Проверка переполнения VRAM (2MB на PSP)
        if (vram_ptr > vram_base + 0x200000) {
            return NULL;
        }
        return result;
    } else {
        // Системная память
        #ifdef __GNUC__
        return memalign(alignment, size);
        #else
        return _aligned_malloc(size, alignment);
        #endif
    }
}

void platform_mem_free(void* ptr, MemoryType type) {
    if (type == MEM_TYPE_SYSTEM) {
        free(ptr);
    }
    // VRAM на PSP не освобождается в простой реализации (bump allocator)
}

bool platform_mem_is_vram(const void* ptr) {
    return ((unsigned int)ptr >= 0x04000000 && (unsigned int)ptr < 0x04200000);
}
```

---

## 5. Стратегия сохранения производительности

### 5.1 Использование `static inline` для hot-path функций

**Проблема:** Косвенные вызовы через HAL добавляют overhead на критичных участках (60 FPS цикл).

**Решение:**
```c
// В video_device.h (публичный API)
static inline void video_device_set_color(VideoDeviceContext* ctx, uint32_t color) {
    // Прямой доступ к полю структуры (если контекст известен на этапе компиляции)
    ctx->current_color = color;
}
```

**Альтернатива для сложных функций:** Макросы препроцессора
```c
// В platforms/psp/psp_video_inline.h
#define VIDEO_DEVICE_SET_BLEND(ctx, mode) \
    do { \
        if ((mode) == BLEND_MODE_ALPHA) { \
            sceGuEnable(GU_BLEND); \
            sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0); \
        } else { \
            sceGuDisable(GU_BLEND); \
        } \
    } while(0)
```

### 5.2 Батчинг через HAL

**Текущая система:** `SpriteBatch` в `graphics.c` накапливает до 128 спрайтов перед отправкой в GPU.

**Изоляция в HAL:**
```c
// video_device.c (реализация)
void video_device_draw_array(VideoDeviceContext* ctx, ...) {
    // Проверка возможности батчинга
    if (ctx->batch.texture == texture && ctx->batch.count < MAX_SPRITES) {
        // Добавить в батч
        ctx->batch.vertices[ctx->batch.count++] = *vertices;
    } else {
        // Сбросить предыдущий батч
        flush_batch(ctx);
        // Начать новый
        ctx->batch.texture = texture;
        ctx->batch.vertices[0] = *vertices;
        ctx->batch.count = 1;
    }
}
```

**Преимущество:** Логика батчинга скрыта от игрового кода, производительность сохранена.

### 5.3 Таблицы указателей на функции

Для динамического переключения платформ (compile-time или runtime):

```c
// video_device.c
typedef struct {
    void (*draw_array)(VideoDeviceContext*, const Vertex2D*, int, VideoTexture*, DrawMode);
    void (*clear)(VideoDeviceContext*, uint32_t);
    // ... остальные функции
} VideoDeviceVTable;

// PSP реализация
static void psp_draw_array(...) { /* sceGuDrawArray код */ }
static void psp_clear(...) { /* sceGuClear код */ }

static const VideoDeviceVTable psp_vtable = {
    .draw_array = psp_draw_array,
    .clear = psp_clear,
    // ...
};

// Выбор VTable на этапе компиляции
#ifdef PSP_PLATFORM
    static const VideoDeviceVTable* vtable = &psp_vtable;
#endif
```

**Overhead:** ~1-2 цикла на непрямой вызов (незначительно при 333 MHz CPU PSP).

---

## 6. Организация файловой структуры

### 6.1 Новая структура проекта

```
bounce_zero/
├── src/                          # Платформенно-независимый код
│   ├── game/                     # Игровая логика
│   │   ├── game.c/h
│   │   ├── physics.c/h
│   │   ├── level.c/h
│   │   ├── menu.c/h
│   │   └── ...
│   ├── hal/                      # Hardware Abstraction Layer
│   │   ├── video_device.h
│   │   ├── input_system.h
│   │   ├── platform_kernel.h
│   │   ├── audio_mixer.h
│   │   └── platform_memory.h
│   └── main.c                    # Общий entry point (вызывает platform_init)
│
├── platforms/
│   ├── psp/                      # PSP-специфичная реализация
│   │   ├── psp_video.c           # sceGu* обёртки
│   │   ├── psp_input.c           # sceCtrl* обёртки
│   │   ├── psp_kernel.c          # sceKernel* обёртки
│   │   ├── psp_audio.c           # pspAudio* обёртки
│   │   ├── psp_memory.c          # VRAM аллокатор
│   │   └── psp_platform.h        # Общий заголовок PSP платформы
│   │
│   └── sdl2/                     # Будущая портация (пример)
│       ├── sdl2_video.c
│       ├── sdl2_input.c
│       └── ...
│
├── tools/                        # Утилиты (без изменений)
├── levels/                       # Ресурсы (без изменений)
└── Makefile                      # Обновлённый для многоплатформенной сборки
```

### 6.2 Makefile для многоплатформенной сборки

```makefile
# Выбор платформы (по умолчанию PSP)
PLATFORM ?= psp

# Общие исходники
COMMON_OBJS = src/game/game.o src/game/physics.o src/game/level.o \
              src/game/menu.o src/main.o

# Платформенно-зависимые исходники
ifeq ($(PLATFORM),psp)
    PLATFORM_OBJS = platforms/psp/psp_video.o \
                    platforms/psp/psp_input.o \
                    platforms/psp/psp_kernel.o \
                    platforms/psp/psp_audio.o \
                    platforms/psp/psp_memory.o
    CFLAGS += -DPSP_PLATFORM
    LIBS = -lpspgu -lpspctrl -lpspaudiolib -lpspkernel
    PSPSDK = $(shell psp-config --pspsdk-path)
    include $(PSPSDK)/lib/build.mak
endif

ifeq ($(PLATFORM),sdl2)
    PLATFORM_OBJS = platforms/sdl2/sdl2_video.o \
                    platforms/sdl2/sdl2_input.o
    CFLAGS += -DSDL2_PLATFORM $(shell sdl2-config --cflags)
    LIBS = $(shell sdl2-config --libs)
endif

OBJS = $(COMMON_OBJS) $(PLATFORM_OBJS)

# Основная цель
TARGET = Bounce
$(TARGET): $(OBJS)
    $(CC) -o $@ $^ $(LIBS)
```

---

## 7. Поэтапный план миграции

### Фаза 1: Подготовка инфраструктуры (1-2 дня)
- [ ] Создать структуру папок `src/hal/` и `platforms/psp/`
- [ ] Написать заголовочные файлы HAL (без реализации)
- [ ] Настроить Makefile для раздельной компиляции

### Фаза 2: Миграция графики (3-5 дней)
- [ ] Реализовать `platforms/psp/psp_video.c` с полным функционалом GU
- [ ] Заменить вызовы `sceGu*` в `graphics.c` на `video_device_*`
- [ ] Протестировать батчинг и производительность (должно быть 60 FPS)
- [ ] Проверить корректность отрисовки текстур и шрифтов

### Фаза 3: Миграция ввода (1 день)
- [ ] Реализовать `psp_input.c` с маппингом кнопок
- [ ] Заменить `SceCtrlData` в `input.c` на `InputState`
- [ ] Протестировать edge-detection и lock механизмы

### Фаза 4: Миграция системных функций (2 дня)
- [ ] Реализовать `psp_kernel.c` (таймеры, callbacks, dcache)
- [ ] Заменить `sceKernel*` в `main.c` на `platform_*`
- [ ] Протестировать выход по HOME-кнопке

### Фаза 5: Миграция памяти (1-2 дня)
- [ ] Реализовать `psp_memory.c` (VRAM аллокатор)
- [ ] Заменить прямое использование `vram*` функций
- [ ] Проверить корректность выравнивания текстур

### Фаза 6: Миграция аудио (2-3 дня)
- [ ] Реализовать `psp_audio.c` (callback-driven микшер)
- [ ] Изолировать OTT парсер от PSP-специфики
- [ ] Протестировать синхронизацию звука

### Фаза 7: Тестирование и оптимизация (2-3 дня)
- [ ] Профилирование с PSP Profiler
- [ ] Оптимизация hot-path функций через `inline`
- [ ] Сравнение производительности с оригиналом (должно быть ≤5% разницы)

**Общая длительность:** ~14-18 дней (1 разработчик)

---

## 8. Риски и их митигация

| Риск | Вероятность | Влияние | Митигация |
|------|-------------|---------|-----------|
| Падение производительности из-за косвенных вызовов | Средняя | Высокое | Использовать `static inline`, макросы, профилирование |
| Ошибки синхронизации dcache | Низкая | Критичное | Автоматический вызов `platform_dcache_writeback()` в HAL |
| Сложность портирования display lists | Высокая | Среднее | Скрыть детали в `psp_video.c`, предоставить буферизацию команд |
| Увеличение размера бинарника | Низкая | Низкое | LTO (Link-Time Optimization), strip неиспользуемых функций |

---

## 9. Метрики успеха

1. **Изоляция зависимостей:**
   - ✅ 0 вхождений `#include <psp*.h>` в папке `src/game/`
   - ✅ 0 вызовов `sce*` функций вне `platforms/psp/`

2. **Производительность:**
   - ✅ Стабильные 60 FPS на PSP-1000 (222 MHz CPU)
   - ✅ Overhead HAL ≤ 5% (измерить через `pspDebugScreenPrintf`)

3. **Портируемость:**
   - ✅ Возможность компиляции для SDL2 без изменения игровой логики
   - ✅ Чистая компиляция с `-Wall -Wextra -pedantic`

4. **Поддержка:**
   - ✅ Документация HAL интерфейсов
   - ✅ Примеры использования для новых платформ

---

## 10. Заключение

Предложенная архитектура обеспечивает:
- **Полную изоляцию** PSP SDK в драйверном слое
- **Сохранение производительности** через inline функции и батчинг
- **Масштабируемость** для портации на SDL2, Nintendo Switch, и другие платформы
- **Минимальные изменения** в игровой логике (только замена API вызовов)

Ключевое преимущество — возможность разработки и тестирования игры на десктопе с последующей компиляцией для PSP без изменений в кодовой базе.

---

**Следующий шаг:** Создание прототипа `video_device.h` + `psp_video.c` для валидации архитектуры.
