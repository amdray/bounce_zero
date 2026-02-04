# Руководство по портированию Bounce Zero на SDL2

**Дата создания:** 2 февраля 2026  
**Версия:** 1.0  
**Цель:** Портирование игры Bounce Zero с PSP SDK на SDL2 для обеспечения кроссплатформенности

---

## Содержание

1. [Обзор текущей архитектуры](#1-обзор-текущей-архитектуры)
2. [Анализ доступных библиотек](#2-анализ-доступных-библиотек)
3. [Архитектура портирования на SDL2](#3-архитектура-портирования-на-sdl2)
4. [Детальный план миграции подсистем](#4-детальный-план-миграции-подсистем)
5. [Стратегия сохранения совместимости с PSP](#5-стратегия-сохранения-совместимости-с-psp)
6. [Дорожная карта реализации](#6-дорожная-карта-реализации)

---

## 1. Обзор текущей архитектуры

### 1.1 PSP SDK зависимости игры

Согласно документу `ARCHITECTURE_REFACTORING.md`, игра использует следующие компоненты PSP SDK:

| Подсистема | Вызовов | Файлы | Критичность |
|------------|---------|-------|-------------|
| **Graphics (GU)** | 52 | graphics.c, png.c, font_atlas.c | ⚠️ МАКСИМАЛЬНАЯ |
| **Display (VSync)** | 3 | graphics.c, save.c | Высокая |
| **Input (Ctrl)** | 3 | input.c | Высокая |
| **Kernel (Threads/Time)** | 12 | main.c, sound.c | ⚠️ МАКСИМАЛЬНАЯ |
| **Memory (VRAM)** | 5 | graphics.c, png.c, font_atlas.c | ⚠️ МАКСИМАЛЬНАЯ |
| **Audio** | 6 | sound.c | Средняя |
| **Utility (Save/Lang)** | 8 | save.c, local.c | Средняя |

### 1.2 Ключевые особенности игры

- **Разрешение:** 480×272 (PSP native)
- **Рендеринг:** 2D спрайтовая графика (Display Lists, батчинг до 128 спрайтов)
- **Текстуры:** PNG с альфа-каналом, индексированные T4 текстуры для шрифтов
- **Аудио:** OTT формат (Nokia ringtone) с wavetable синтезом, 44.1 кГц, 3 канала микширования
- **Физика:** Fixed timestep 30ms (независимо от FPS)
- **Инпут:** Только цифровые кнопки (аналоговый стик не используется)

---

## 2. Анализ доступных библиотек

### 2.1 SDL2 экосистема для PSP

Из `psp-packages-master` доступны следующие пакеты SDL2:

#### Основные библиотеки
- **sdl2** (v2.32.8) — базовая библиотека
  - Видео, аудио, ввод, таймеры
  - Зависимости: `libpspvram`, `pspgl`
  
- **sdl2-image** — загрузка изображений
  - Поддержка PNG, JPEG, BMP и других форматов
  - Прямая замена текущего `png.c`
  
- **sdl2-mixer** — аудио микшер
  - Поддержка WAV, OGG, MP3, MOD
  - ⚠️ НЕ подходит для OTT формата — требуется кастомный микшер
  
- **sdl2-ttf** — рендеринг TrueType шрифтов
  - Зависимость: `freetype2`
  - ⚠️ Игра использует bitmap-атласы — не требуется
  
- **sdl2-gfx** — примитивы и эффекты
  - Линии, круги, полигоны, вращение
  - ⚠️ Избыточно для простого спрайтового рендера
  
- **sdl2-net** — сетевые функции
  - ❌ Не используется в игре

#### Вспомогательные библиотеки
- **libpng** (v1.6.53) — PNG кодек (используется SDL2_image)
- **zlib** — компрессия (зависимость libpng)
- **freetype2** — рендеринг шрифтов
- **libogg/libvorbis** — аудио кодеки

### 2.2 Преимущества SDL2

✅ **Кроссплатформенность**
- Windows, Linux, macOS, Android, iOS, PSP, PS Vita
- Единый API для всех платформ

✅ **Производительность**
- Hardware-accelerated рендеринг через `SDL_Renderer`
- Батчинг примитивов и текстур
- VSync и frame pacing

✅ **Простота интеграции**
- Замена `sceGu*` → `SDL_Render*`
- Замена `sceCtrl*` → `SDL_GetKeyboardState()` / `SDL_GameController`
- Замена `sceKernelGetSystemTimeWide()` → `SDL_GetTicks64()`

✅ **Меньше платформенного кода**
- Автоматическое управление памятью
- Нет необходимости в ручном управлении VRAM
- Встроенная обработка событий

---

## 3. Архитектура портирования на SDL2

### 3.1 Структура HAL (Hardware Abstraction Layer)

```
┌─────────────────────────────────────────┐
│    Игровая логика (game.c, menu.c...)   │
│    ✅ НЕ требует изменений               │
└──────────────────┬──────────────────────┘
                   │
┌──────────────────▼──────────────────────┐
│   Абстрактные интерфейсы (HAL)          │
│   - graphics.h    → графика             │
│   - input.h       → ввод                │
│   - sound.h       → аудио               │
│   - (новые файлы не создаём)            │
└──────────────────┬──────────────────────┘
                   │
        ┌──────────┴──────────┐
        │                     │
┌───────▼─────┐      ┌────────▼────────┐
│PSP Backend  │      │ SDL2 Backend    │
│(текущий)    │      │ (новый)         │
│             │      │                 │
│graphics.c   │      │graphics_sdl.c   │
│input.c      │      │input_sdl.c      │
│sound.c      │      │sound_sdl.c      │
│             │      │                 │
│sceGu*, etc. │      │SDL_Render*, etc.│
└─────────────┘      └─────────────────┘
```

### 3.2 Выбор варианта портирования

#### Вариант A: Полная замена (рекомендуется для начала)
- Создать новые файлы `graphics_sdl.c`, `input_sdl.c`, `sound_sdl.c`
- Переключение через `#ifdef USE_SDL2` в makefile
- Сохраняет PSP бэкенд для сравнения и отладки

#### Вариант B: Постепенная миграция
- Замена функций по одной подсистеме
- Создать обёртки над SDL2 с сигнатурами как у PSP SDK
- Минимизирует риски, но требует больше времени

#### Вариант C: Полный рефакторинг (из ARCHITECTURE_REFACTORING.md)
- Создать чистый HAL с `video_device.h`, `input_system.h`, `audio_mixer.h`
- Два бэкенда: `platforms/psp/` и `platforms/sdl2/`
- ✅ Максимальная переносимость
- ⚠️ Большой объём работы

### 3.3 Рекомендуемый подход: Вариант A + элементы C

1. **Этап 1:** Создать SDL2 версии с сохранением интерфейсов `graphics.h`, `input.h`, `sound.h`
2. **Этап 2:** Постепенно рефакторить HAL (если потребуется ещё платформа)
3. **Этап 3:** Унифицировать сохранения и локализацию

---

## 4. Детальный план миграции подсистем

### 4.1 Графическая подсистема (graphics.c → graphics_sdl.c)

#### Текущая реализация (PSP GU)
```c
// Инициализация
sceGuInit();
sceGuStart(GU_DIRECT, list);
sceGuDrawBuffer(GU_PSM_8888, draw_buffer, 512);
sceGuDispBuffer(480, 272, disp_buffer, 512);

// Рендеринг спрайтов
sceGuStart(GU_DIRECT, list);
sceGuTexImage(0, tex->stride, tex->height, tex->stride, tex->data);
sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF,
               vertex_count, 0, vertices);
sceGuFinish();
sceGuSync(0, 0);
sceGuSwapBuffers();
```

#### SDL2 реализация
```c
// Инициализация
SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER);
SDL_Window* window = SDL_CreateWindow("Bounce Zero",
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    480, 272, SDL_WINDOW_SHOWN);
    
#ifdef __PSP__
// Полноэкранный режим для PSP
SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
#endif

SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
    SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

// Рендеринг спрайтов (batch система)
SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
SDL_Rect src = {sprite_x, sprite_y, sprite_w, sprite_h};
SDL_Rect dst = {screen_x, screen_y, sprite_w, sprite_h};
SDL_RenderCopy(renderer, texture, &src, &dst);

// Вместо batching можно использовать SDL_RenderGeometry (SDL 2.0.18+)
SDL_RenderGeometry(renderer, texture, vertices, vertex_count, NULL, 0);

// Swap buffers
SDL_RenderPresent(renderer);
```

#### Маппинг функций

| PSP GU функция | SDL2 аналог | Примечание |
|----------------|-------------|------------|
| `sceGuInit()` | `SDL_CreateRenderer()` | Инициализация рендера |
| `sceGuStart()/Finish()` | `SDL_RenderClear()` / `SDL_RenderPresent()` | Начало/конец кадра |
| `sceGuDrawArray(GU_SPRITES)` | `SDL_RenderCopy()` или `SDL_RenderGeometry()` | Рендеринг спрайтов |
| `sceGuSwapBuffers()` | `SDL_RenderPresent()` | Обмен буферов |
| `sceDisplayWaitVblankStart()` | `SDL_RENDERER_PRESENTVSYNC` | VSync |
| `sceGuClear()` | `SDL_RenderClear()` | Очистка экрана |
| `sceGuBlendFunc()` | `SDL_SetTextureBlendMode()` | Альфа-блендинг |
| `sceGuScissor()` | `SDL_RenderSetClipRect()` | Clipping |
| `sceGuGetMemory()` | Локальный буфер вершин | Batch система |

#### Батчинг в SDL2

PSP версия использует `MAX_SPRITES_PER_BATCH = 128` для минимизации вызовов.

**Решение 1: SDL_RenderGeometry (SDL 2.0.18+, доступно в 2.32.8)**
```c
// Накапливаем вершины в буфере
typedef struct {
    SDL_Vertex vertices[MAX_SPRITES_PER_BATCH * 6]; // 2 треугольника на спрайт
    int count;
    SDL_Texture* texture;
} SpriteBatch;

void batch_add_sprite(SpriteBatch* batch, SDL_Texture* tex,
                      float x, float y, float w, float h,
                      float u0, float v0, float u1, float v1,
                      SDL_Color color) {
    if (batch->count >= MAX_SPRITES_PER_BATCH || batch->texture != tex) {
        batch_flush(batch);
        batch->texture = tex;
    }
    
    // 2 треугольника (0-1-2, 2-3-0)
    SDL_Vertex* v = &batch->vertices[batch->count * 6];
    
    // Top-left
    v[0].position = (SDL_FPoint){x, y};
    v[0].color = color;
    v[0].tex_coord = (SDL_FPoint){u0, v0};
    
    // Top-right
    v[1].position = (SDL_FPoint){x + w, y};
    v[1].color = color;
    v[1].tex_coord = (SDL_FPoint){u1, v0};
    
    // Bottom-right
    v[2].position = (SDL_FPoint){x + w, y + h};
    v[2].color = color;
    v[2].tex_coord = (SDL_FPoint){u1, v1};
    
    // Bottom-right (повтор для второго треугольника)
    v[3] = v[2];
    
    // Bottom-left
    v[4].position = (SDL_FPoint){x, y + h};
    v[4].color = color;
    v[4].tex_coord = (SDL_FPoint){u0, v1};
    
    // Top-left (замыкание)
    v[5] = v[0];
    
    batch->count++;
}

void batch_flush(SpriteBatch* batch) {
    if (batch->count == 0) return;
    
    SDL_RenderGeometry(renderer, batch->texture,
                       batch->vertices, batch->count * 6,
                       NULL, 0);
    batch->count = 0;
}
```

**Решение 2: SDL_RenderCopy в цикле (проще, но медленнее)**
```c
for (int i = 0; i < sprite_count; i++) {
    SDL_RenderCopy(renderer, texture, &src_rects[i], &dst_rects[i]);
}
```

#### Текстуры и форматы

| PSP формат | SDL2 эквивалент |
|------------|-----------------|
| `GU_PSM_8888` (RGBA) | `SDL_PIXELFORMAT_ABGR8888` |
| `GU_PSM_T4` (4-bit indexed) | `SDL_PIXELFORMAT_INDEX4LSB` + палитра |

**Загрузка PNG (замена png.c):**
```c
#include <SDL_image.h>

SDL_Surface* load_png(const char* path) {
    SDL_Surface* surface = IMG_Load(path);
    if (!surface) {
        fprintf(stderr, "IMG_Load error: %s\n", IMG_GetError());
        return NULL;
    }
    return surface;
}

SDL_Texture* create_texture_from_png(SDL_Renderer* renderer, const char* path) {
    SDL_Surface* surface = load_png(path);
    if (!surface) return NULL;
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    
    return texture;
}
```

#### Шрифты (T4 палитра → SDL палитра)

Текущая реализация использует `sceGuClutMode()` для 4-bit индексированных текстур.

```c
// PSP версия
sceGuClutMode(GU_PSM_8888, 0, 0xFF, 0);
sceGuClutLoad(2, font_clut); // 16 цветов

// SDL2 версия (создание палитры)
SDL_Surface* create_indexed_surface(int w, int h, uint32_t* palette, int num_colors) {
    SDL_Surface* surface = SDL_CreateRGBSurface(0, w, h, 4, 0, 0, 0, 0);
    
    SDL_Color colors[16];
    for (int i = 0; i < num_colors; i++) {
        colors[i].r = (palette[i] >> 0) & 0xFF;
        colors[i].g = (palette[i] >> 8) & 0xFF;
        colors[i].b = (palette[i] >> 16) & 0xFF;
        colors[i].a = (palette[i] >> 24) & 0xFF;
    }
    
    SDL_SetPaletteColors(surface->format->palette, colors, 0, num_colors);
    
    return surface;
}
```

#### Управление памятью

PSP использует `sceGeEdramGetAddr()` для VRAM аллокации.  
SDL2 автоматически управляет текстурами — **упрощение!**

```c
// PSP: ручное управление VRAM
void* vram_ptr = getStaticVramTexture(width, height, GU_PSM_8888);
sceKernelDcacheWritebackRange(vram_ptr, size);

// SDL2: автоматическое управление
SDL_Texture* texture = SDL_CreateTexture(renderer,
    SDL_PIXELFORMAT_ABGR8888,
    SDL_TEXTUREACCESS_STATIC,
    width, height);
    
SDL_UpdateTexture(texture, NULL, pixels, pitch);
// Нет необходимости в ручном writeback!
```

---

### 4.2 Подсистема ввода (input.c → input_sdl.c)

#### Текущая реализация (PSP Ctrl)
```c
// Инициализация
sceCtrlSetSamplingCycle(0);
sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

// Опрос
SceCtrlData pad;
sceCtrlReadBufferPositive(&pad, 1);
unsigned int buttons = pad.Buttons;

// Проверка нажатия
if (buttons & PSP_CTRL_CROSS) { /* действие */ }
```

#### SDL2 реализация

**Вариант 1: Клавиатура (для десктопа)**
```c
const Uint8* keys = SDL_GetKeyboardState(NULL);

if (keys[SDL_SCANCODE_UP])    { /* вверх */ }
if (keys[SDL_SCANCODE_DOWN])  { /* вниз */ }
if (keys[SDL_SCANCODE_SPACE]) { /* прыжок (CROSS) */ }
```

**Вариант 2: GameController API (универсальный)**
```c
// Инициализация
SDL_GameController* controller = NULL;
if (SDL_NumJoysticks() > 0) {
    controller = SDL_GameControllerOpen(0);
}

// Опрос кнопок
bool button_a = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A);
bool dpad_up = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP);
```

**Вариант 3: События (для меню)**
```c
SDL_Event event;
while (SDL_PollEvent(&event)) {
    if (event.type == SDL_CONTROLLERBUTTONDOWN) {
        if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
            // Нажата кнопка A (CROSS на PSP)
        }
    }
}
```

#### Маппинг кнопок PSP → SDL2

| PSP кнопка | SDL_GameController | Клавиатура (десктоп) |
|------------|-------------------|---------------------|
| `PSP_CTRL_UP` | `SDL_CONTROLLER_BUTTON_DPAD_UP` | `SDL_SCANCODE_UP` / `W` |
| `PSP_CTRL_DOWN` | `SDL_CONTROLLER_BUTTON_DPAD_DOWN` | `SDL_SCANCODE_DOWN` / `S` |
| `PSP_CTRL_LEFT` | `SDL_CONTROLLER_BUTTON_DPAD_LEFT` | `SDL_SCANCODE_LEFT` / `A` |
| `PSP_CTRL_RIGHT` | `SDL_CONTROLLER_BUTTON_DPAD_RIGHT` | `SDL_SCANCODE_RIGHT` / `D` |
| `PSP_CTRL_CROSS` | `SDL_CONTROLLER_BUTTON_A` | `SDL_SCANCODE_SPACE` |
| `PSP_CTRL_CIRCLE` | `SDL_CONTROLLER_BUTTON_B` | `SDL_SCANCODE_ESCAPE` |
| `PSP_CTRL_START` | `SDL_CONTROLLER_BUTTON_START` | `SDL_SCANCODE_RETURN` |
| `PSP_CTRL_SELECT` | `SDL_CONTROLLER_BUTTON_BACK` | `SDL_SCANCODE_BACKSPACE` |

#### Рефакторинг input.c

Текущий `input.c` использует битовые маски PSP. Нужна обёртка:

```c
// input_sdl.c
#include "input.h"
#include <SDL.h>

static SDL_GameController* s_controller = NULL;
static const Uint8* s_keys = NULL;

void input_init(void) {
    if (SDL_NumJoysticks() > 0) {
        s_controller = SDL_GameControllerOpen(0);
    }
}

void input_update(void) {
    s_keys = SDL_GetKeyboardState(NULL);
    
    // Маппинг PSP кнопок на SDL
    unsigned int sdl_buttons = 0;
    
    if (s_controller) {
        if (SDL_GameControllerGetButton(s_controller, SDL_CONTROLLER_BUTTON_DPAD_UP))
            sdl_buttons |= INPUT_BUTTON_UP;
        if (SDL_GameControllerGetButton(s_controller, SDL_CONTROLLER_BUTTON_A))
            sdl_buttons |= INPUT_BUTTON_CROSS;
        // ... и т.д.
    }
    
    // Клавиатура как fallback
    if (s_keys[SDL_SCANCODE_UP] || s_keys[SDL_SCANCODE_W])
        sdl_buttons |= INPUT_BUTTON_UP;
    if (s_keys[SDL_SCANCODE_SPACE])
        sdl_buttons |= INPUT_BUTTON_CROSS;
    // ... и т.д.
    
    // Обновляем состояние (логика из оригинального input.c)
    s_prev_buttons = s_buttons;
    s_buttons = sdl_buttons;
    // ... обработка pressed/released/accumulated
}
```

#### Сохранение совместимости

Хедер `input.h` **НЕ меняется**:
```c
// input.h (без изменений)
typedef enum {
    INPUT_BUTTON_UP    = 1 << 0,
    INPUT_BUTTON_DOWN  = 1 << 1,
    // ... как в текущей версии
} InputButton;

void input_init(void);
void input_update(void);
bool input_pressed(unsigned int button);
bool input_held(unsigned int button);
```

Игровая логика продолжает использовать `input_pressed(INPUT_BUTTON_CROSS)` без изменений.

---

### 4.3 Подсистема аудио (sound.c → sound_sdl.c)

#### Текущая реализация (PSP Audio)
```c
// Инициализация
pspAudioInit();
pspAudioSetChannelCallback(0, ott_audio_callback, NULL);

// Callback (вызывается системой ~60 раз/сек)
void ott_audio_callback(void* buf, unsigned int length, void* userdata) {
    psp_sample_t* samples = (psp_sample_t*)buf;
    for (unsigned int i = 0; i < length; i++) {
        // Генерация микса от 3 OTT плееров
        short mix = (generate_player_sample(&g_hoop_player) +
                     generate_player_sample(&g_pickup_player) +
                     generate_player_sample(&g_pop_player)) / active_channels;
        samples[i].l = samples[i].r = mix;
    }
}
```

#### SDL2 реализация

**SDL_AudioSpec и callback:**
```c
#include <SDL.h>

static SDL_AudioDeviceID s_audio_device;

// Callback (тот же формат что и PSP)
void sdl_audio_callback(void* userdata, Uint8* stream, int len) {
    int16_t* samples = (int16_t*)stream;
    int sample_count = len / (2 * sizeof(int16_t)); // Stereo
    
    for (int i = 0; i < sample_count; i++) {
        // Та же логика микширования что и в PSP версии
        short mix = (generate_player_sample(&g_hoop_player) +
                     generate_player_sample(&g_pickup_player) +
                     generate_player_sample(&g_pop_player)) / active_channels;
        
        samples[i * 2 + 0] = mix; // Left
        samples[i * 2 + 1] = mix; // Right
    }
}

void sound_init(void) {
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 44100;
    want.format = AUDIO_S16SYS; // Signed 16-bit, native endian
    want.channels = 2; // Stereo
    want.samples = 512; // Размер буфера (можно настроить)
    want.callback = sdl_audio_callback;
    want.userdata = NULL;
    
    s_audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (s_audio_device == 0) {
        fprintf(stderr, "SDL_OpenAudioDevice error: %s\n", SDL_GetError());
        return;
    }
    
    // Запуск воспроизведения
    SDL_PauseAudioDevice(s_audio_device, 0);
}
```

#### Сохранение OTT формата

**Хорошая новость:** Вся логика OTT парсинга и wavetable синтеза остаётся без изменений!

Файлы, которые **НЕ требуют** изменений:
- `parse_ringtone()` — парсинг OTT
- `init_wavetable()` — генерация синусоиды
- `generate_player_sample()` — синтез звука
- Структуры `ott_info_t`, `ott_player_t`

**Единственное изменение:** замена `pspAudioSetChannelCallback()` на `SDL_OpenAudioDevice()`.

#### Управление громкостью

```c
// PSP версия
pspAudioSetVolume(0, volume, volume);

// SDL2 версия
// Можно использовать SDL_MixAudio() или умножение семплов
void sound_set_volume(float volume) { // 0.0 - 1.0
    g_master_volume = volume;
}

// В callback'е:
short mix = (short)(raw_sample * g_master_volume);
```

#### SDL2_mixer альтернатива

⚠️ **НЕ рекомендуется** для Bounce Zero, потому что:
- SDL2_mixer поддерживает WAV/OGG/MP3, но **не OTT**
- Потребуется конвертация OTT → WAV (усложнение)
- Текущий wavetable синтез эффективнее предзаписанных файлов

**Вывод:** Использовать низкоуровневый `SDL_AudioSpec` callback + текущий синтезатор.

---

### 4.4 Таймеры и игровой цикл (main.c)

#### Текущая реализация (PSP Kernel)
```c
unsigned long long prev_time_ms = sceKernelGetSystemTimeWide() / 1000ULL;

while (running) {
    unsigned long long now_ms = sceKernelGetSystemTimeWide() / 1000ULL;
    unsigned long long delta_ms = now_ms - prev_time_ms;
    
    // Fixed timestep 30ms для физики
    while (delta_ms >= 30) {
        game_physics_tick();
        delta_ms -= 30;
        prev_time_ms += 30;
    }
    
    // Рендеринг
    game_render();
}
```

#### SDL2 реализация
```c
#include <SDL.h>

Uint64 prev_time_ms = SDL_GetTicks64();

while (running) {
    Uint64 now_ms = SDL_GetTicks64();
    Uint64 delta_ms = now_ms - prev_time_ms;
    
    // Та же логика fixed timestep
    while (delta_ms >= 30) {
        game_physics_tick();
        delta_ms -= 30;
        prev_time_ms += 30;
    }
    
    // Рендеринг
    game_render();
    
    // Обработка событий (quit, resize, и т.д.)
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running = false;
        }
    }
}
```

#### VSync

```c
// PSP версия
sceDisplayWaitVblankStart();

// SDL2 версия
// Автоматически через флаг SDL_RENDERER_PRESENTVSYNC
SDL_RenderPresent(renderer); // Блокируется до VBlank
```

#### Callback выхода (HOME кнопка)

PSP использует `sceKernelCreateCallback()` для обработки HOME.

```c
// PSP версия
int exit_callback(int arg1, int arg2, void* common) {
    g_game.state = STATE_EXIT;
    return 0;
}

// SDL2 версия (через события)
SDL_Event event;
while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) { // Window close или Ctrl+C
        g_game.state = STATE_EXIT;
    }
    
#ifdef __PSP__
    // HOME кнопка на PSP генерирует SDL_QUIT
    if (event.type == SDL_CONTROLLERBUTTONDOWN &&
        event.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE) {
        g_game.state = STATE_EXIT;
    }
#endif
}
```

---

### 4.5 Система сохранений (save.c)

#### Текущая реализация (PSP Utility)
```c
// PSP использует GUI диалог сохранения
sceUtilitySavedataInitStart(&save_params);
while (status != PSP_UTILITY_DIALOG_QUIT) {
    status = sceUtilitySavedataGetStatus();
    sceUtilitySavedataUpdate(1);
    // Рендеринг диалога...
}
```

#### SDL2 реализация

SDL не имеет встроенных диалогов сохранения. Варианты:

**Вариант 1: Прямая запись в файл (десктоп)**
```c
#include <SDL.h>

const char* get_save_path(void) {
#ifdef __PSP__
    return "ms0:/PSP/SAVEDATA/BOUNCE00/save.dat";
#else
    static char path[256];
    char* pref_path = SDL_GetPrefPath("BounceZero", "SaveData");
    snprintf(path, sizeof(path), "%ssave.dat", pref_path);
    SDL_free(pref_path);
    return path;
#endif
}

int save_game(const SaveData* data) {
    const char* path = get_save_path();
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    
    fwrite(data, sizeof(SaveData), 1, f);
    fclose(f);
    
    return 0;
}
```

**Вариант 2: SDL_RWops (кроссплатформенный)**
```c
int save_game(const SaveData* data) {
    SDL_RWops* file = SDL_RWFromFile(get_save_path(), "wb");
    if (!file) return -1;
    
    SDL_RWwrite(file, data, sizeof(SaveData), 1);
    SDL_RWclose(file);
    
    return 0;
}
```

**Вариант 3: Сохранение GUI для PSP (опционально)**
```c
#ifdef __PSP__
    // Использовать sceUtilitySavedata* для PSP
    sceUtilitySavedataInitStart(&params);
    // ... текущая логика
#else
    // Прямая запись для других платформ
    save_game(&data);
#endif
```

---

### 4.6 Локализация (local.c)

#### Текущая реализация
```c
// Определение языка через PSP Utility
int lang_id;
sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &lang_id);

// Загрузка файла lang/lang.en, lang.ru-RU, и т.д.
```

#### SDL2 реализация
```c
#include <SDL_locale.h>

const char* get_system_language(void) {
#ifdef __PSP__
    int lang_id;
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &lang_id);
    
    // Маппинг PSP языков
    switch (lang_id) {
        case PSP_SYSTEMPARAM_LANGUAGE_ENGLISH: return "en";
        case PSP_SYSTEMPARAM_LANGUAGE_RUSSIAN: return "ru-RU";
        case PSP_SYSTEMPARAM_LANGUAGE_GERMAN: return "de";
        // ... и т.д.
        default: return "en";
    }
#else
    // SDL 2.0.14+ (2.32.8 имеет это)
    SDL_Locale* locales = SDL_GetPreferredLocales();
    if (locales && locales[0].language) {
        static char lang_code[8];
        if (locales[0].country) {
            snprintf(lang_code, sizeof(lang_code), "%s-%s",
                     locales[0].language, locales[0].country);
        } else {
            snprintf(lang_code, sizeof(lang_code), "%s", locales[0].language);
        }
        SDL_free(locales);
        return lang_code;
    }
    return "en"; // Fallback
#endif
}
```

---

## 5. Стратегия сохранения совместимости с PSP

### 5.1 Использование препроцессора

```c
// Общий заголовок platform.h
#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef USE_SDL2
    #include <SDL.h>
    #include <SDL_image.h>
    // SDL2 специфичные определения
    typedef SDL_Renderer* GraphicsContext;
    typedef SDL_Texture* TextureHandle;
#else
    #include <pspgu.h>
    #include <pspdisplay.h>
    // PSP специфичные определения
    typedef void* GraphicsContext;
    typedef struct texture_t* TextureHandle;
#endif

#endif // PLATFORM_H
```

### 5.2 Структура файлов

```
src/
├── common/              # Платформонезависимый код
│   ├── game.c
│   ├── menu.c
│   ├── physics.c
│   ├── level.c
│   └── ...
│
├── platform/
│   ├── psp/            # PSP backend
│   │   ├── graphics_psp.c
│   │   ├── input_psp.c
│   │   └── sound_psp.c
│   │
│   └── sdl2/           # SDL2 backend
│       ├── graphics_sdl.c
│       ├── input_sdl.c
│       └── sound_sdl.c
│
└── include/            # Общие заголовки
    ├── graphics.h
    ├── input.h
    ├── sound.h
    └── platform.h
```

### 5.3 Makefile конфигурация

```makefile
# Переключение между платформами
ifeq ($(USE_SDL2), 1)
    # SDL2 версия
    CFLAGS += -DUSE_SDL2
    LIBS += -lSDL2 -lSDL2_image
    SRC += src/platform/sdl2/graphics_sdl.c \
           src/platform/sdl2/input_sdl.c \
           src/platform/sdl2/sound_sdl.c
else
    # PSP версия (текущая)
    CFLAGS += -I$(PSPSDK)/include
    LIBS += -lpspgu -lpspdisplay -lpspaudiolib
    SRC += src/platform/psp/graphics_psp.c \
           src/platform/psp/input_psp.c \
           src/platform/psp/sound_psp.c
endif

# Общие файлы (без изменений)
SRC += src/common/game.c \
       src/common/menu.c \
       src/common/physics.c
```

### 5.4 Сборка для разных платформ

```bash
# PSP сборка (текущая)
make clean
make

# SDL2 сборка (PSP)
make clean
make USE_SDL2=1

# SDL2 сборка (Windows/Linux)
make clean
make USE_SDL2=1 TARGET=desktop
```

---

## 6. Дорожная карта реализации

### Фаза 1: Подготовка и изоляция (1-2 недели)
- [ ] Создать структуру `src/platform/psp/` и `src/platform/sdl2/`
- [ ] Переместить текущие `graphics.c`, `input.c`, `sound.c` в `platform/psp/`
- [ ] Создать общие заголовки `graphics.h`, `input.h`, `sound.h` с платформонезависимыми интерфейсами
- [ ] Обновить Makefile для поддержки `USE_SDL2` флага
- [ ] Проверить, что PSP сборка продолжает работать

### Фаза 2: SDL2 графическая подсистема (2-3 недели)
- [ ] Создать `graphics_sdl.c` с базовой инициализацией SDL2
- [ ] Реализовать `SDL_CreateRenderer()` с конфигурацией 480×272
- [ ] Портировать функции очистки экрана и swap buffers
- [ ] Реализовать загрузку PNG через SDL2_image
- [ ] Портировать батчинг спрайтов через `SDL_RenderGeometry()`
- [ ] Реализовать поддержку T4 индексированных шрифтов
- [ ] Тестирование рендеринга: меню, уровни, анимации

### Фаза 3: SDL2 подсистема ввода (1 неделя)
- [ ] Создать `input_sdl.c` с SDL_GameController API
- [ ] Реализовать маппинг PSP кнопок → SDL кнопки
- [ ] Добавить поддержку клавиатуры для десктопа
- [ ] Портировать логику edge-detection (pressed/released/held)
- [ ] Тестирование навигации в меню и управления в игре

### Фаза 4: SDL2 подсистема аудио (1-2 недели)
- [ ] Создать `sound_sdl.c` с SDL_AudioSpec callback
- [ ] Портировать wavetable синтез и OTT парсер (без изменений логики)
- [ ] Реализовать микширование 3 каналов
- [ ] Добавить управление громкостью
- [ ] Тестирование звуков: hoop, pickup, pop

### Фаза 5: Таймеры и игровой цикл (3-5 дней)
- [ ] Заменить `sceKernelGetSystemTimeWide()` на `SDL_GetTicks64()`
- [ ] Портировать fixed timestep логику (30ms)
- [ ] Реализовать обработку SDL_QUIT события
- [ ] Тестирование физического тика и синхронизации

### Фаза 6: Сохранения и локализация (1 неделя)
- [ ] Портировать `save.c` на `SDL_GetPrefPath()` / `SDL_RWops`
- [ ] Реализовать `#ifdef __PSP__` для PSP диалога сохранения
- [ ] Портировать `local.c` на `SDL_GetPreferredLocales()`
- [ ] Тестирование сохранений на разных платформах

### Фаза 7: Отладка и оптимизация (2-3 недели)
- [ ] Профилирование производительности SDL2 на PSP
- [ ] Оптимизация батчинга и размера буферов
- [ ] Тестирование на десктопе (Windows/Linux)
- [ ] Исправление багов и edge cases
- [ ] Сравнение с оригинальной PSP версией

### Фаза 8: Документация и релиз (1 неделя)
- [ ] Обновить README с инструкциями сборки SDL2
- [ ] Создать CMakeLists.txt для кроссплатформенной сборки
- [ ] Написать руководство по портированию на новые платформы
- [ ] Релиз SDL2 версии

**Общее время:** ~10-14 недель (2.5-3.5 месяца)

---

## 7. Преимущества SDL2 портирования

### 7.1 Кроссплатформенность

✅ **PSP** — Полная совместимость через `psp-packages-master/sdl2`  
✅ **PS Vita** — SDL2 имеет нативную поддержку  
✅ **Windows/Linux/macOS** — Десктопная версия для разработки и тестирования  
✅ **Android/iOS** — Потенциал мобильного порта  
✅ **Nintendo Switch** (homebrew) — SDL2 поддерживается

### 7.2 Упрощение разработки

✅ **Меньше платформенного кода** — SDL2 абстрагирует низкоуровневые API  
✅ **Автоматическое управление памятью** — Нет необходимости в VRAM аллокаторах  
✅ **Встроенная загрузка ресурсов** — SDL2_image заменяет кастомный `png.c`  
✅ **Отладка на десктопе** — Быстрый цикл разработки без PPSSPP эмулятора

### 7.3 Производительность

✅ **Hardware acceleration** — SDL2 использует GPU на всех платформах  
✅ **Батчинг через RenderGeometry** — Эффективность как у PSP Display Lists  
✅ **VSync и frame pacing** — Встроенная поддержка без ручной синхронизации

### 7.4 Сообщество и поддержка

✅ **Активное развитие** — SDL2 обновляется регулярно (2.32.8 — актуальная версия)  
✅ **Обширная документация** — https://wiki.libsdl.org/SDL2/  
✅ **Большое сообщество** — Множество примеров и туториалов

---

## 8. Потенциальные проблемы и решения

### 8.1 Производительность на PSP

**Проблема:** SDL2 может быть медленнее нативного GU на PSP.

**Решения:**
- Использовать `SDL_RENDERER_ACCELERATED` для hardware rendering
- Оптимизировать батчинг: `MAX_SPRITES_PER_BATCH = 256` (больше чем 128)
- Уменьшить размер буферов аудио (512 → 256 samples)
- Профилировать с помощью PSP таймеров

### 8.2 Размер текстур

**Проблема:** SDL2 может не требовать степени двойки для stride.

**Решение:**
- Оставить текущую логику выравнивания в `png.c`
- SDL2 автоматически обрабатывает любые размеры

### 8.3 Индексированные текстуры (T4)

**Проблема:** SDL2 имеет ограниченную поддержку палитр.

**Решение:**
- Конвертировать T4 → RGBA при загрузке (один раз)
- Или использовать `SDL_PIXELFORMAT_INDEX4LSB` + `SDL_SetPaletteColors()`

### 8.4 OTT формат аудио

**Проблема:** SDL2_mixer не поддерживает OTT.

**Решение:**
- Использовать низкоуровневый `SDL_AudioSpec` callback
- Сохранить текущий wavetable синтезатор без изменений

---

## 9. Альтернативные подходы

### 9.1 SDL1.2 вместо SDL2

**Плюсы:**
- Проще и легче
- Меньше overhead

**Минусы:**
- Устаревшая (последний релиз 2012)
- Нет hardware acceleration
- Нет GameController API

**Вывод:** ❌ Не рекомендуется. SDL2 — современный стандарт.

### 9.2 Прямой OpenGL/OpenGL ES

**Плюсы:**
- Максимальная производительность
- Полный контроль рендеринга

**Минусы:**
- Сложнее портировать
- Требует написания шейдеров
- Больше платформенного кода

**Вывод:** ⚠️ Избыточно для 2D спрайтовой игры. SDL2 Renderer достаточен.

### 9.3 Движки (Unity, Godot, etc.)

**Плюсы:**
- Готовые инструменты
- Редактор уровней

**Минусы:**
- Нужно переписывать всю игру с нуля
- Большой размер исполнимого файла
- Может не поддерживать PSP

**Вывод:** ❌ Не подходит для портирования существующей кодовой базы.

---

## 10. Заключение

### 10.1 Рекомендации

✅ **Использовать SDL2** для портирования Bounce Zero  
✅ **Сохранить PSP бэкенд** для сравнения и совместимости  
✅ **Постепенная миграция** через `#ifdef USE_SDL2`  
✅ **Приоритет: графика → ввод → аудио → остальное**

### 10.2 Ожидаемые результаты

После портирования на SDL2:
- ✅ Игра запускается на PSP, Windows, Linux, macOS
- ✅ Тот же gameplay и производительность
- ✅ Упрощённая кодовая база (меньше платформенного кода)
- ✅ Возможность портирования на новые платформы (Vita, Switch, Android)

### 10.3 Дальнейшее развитие

После успешного SDL2 портирования можно:
- Добавить поддержку разных разрешений (720p, 1080p)
- Реализовать масштабирование интерфейса
- Добавить поддержку геймпадов (Xbox, PlayStation)
- Портировать на мобильные платформы (сенсорное управление)
- Создать уровневый редактор

---

## Приложения

### A. Ссылки на документацию

- **SDL2 Wiki:** https://wiki.libsdl.org/SDL2/
- **SDL2_image:** https://wiki.libsdl.org/SDL2_image/
- **SDL2 PSP Port:** https://github.com/pspdev/SDL2
- **PSP SDK:** https://github.com/pspdev/pspsdk

### B. Примеры кода

См. директорию `psp-packages-master/sdl2/` для sample проектов:
- `CMakeLists.txt.sample` — Пример сборки
- `main.c.sample` — Минимальный SDL2 проект для PSP

### C. Полезные инструменты

- **PPSSPP** — Эмулятор PSP для быстрого тестирования
- **SDL2_gfx** — Дополнительные примитивы (не требуется для Bounce)
- **stb_image** — Альтернатива SDL2_image (один заголовочный файл)

---

**Автор:** GitHub Copilot  
**Контакт:** Создано на основе анализа проекта Bounce Zero  
**Лицензия:** Следует лицензии проекта Bounce Zero
