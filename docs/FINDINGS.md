# Находки: PSP SDK оптимизации, мусор и float-значения


## Производительность / упрощение через нативные API

- Рендер текста/чисел идёт по пикселю через `graphics_draw_rect`, что вызывает сотни/тысячи вызовов `sceGuDrawArray` на строку. GU заметно эффективнее с батчингом спрайтов из атласа.
  - Код: `src/graphics.c:115`, `src/graphics.c:233`, `src/graphics.c:338`
  - Рекомендация: сделать атлас шрифтов (GU_PSM_T4/T8 + CLUT) и рисовать через `graphics_batch_sprite`.

- Звук микшируется вручную в одном канале. В `pspaudiolib` есть 4 канала с колбэками — можно развести hoop/pickup/pop по каналам и упростить микшер.
  - Код: `src/sound.c:355`, `src/sound.c:454`
  - SDK: `pspsdk-master/src/audio/pspaudiolib.h:20`

- Текстуры грузятся без swizzle (`sceGuTexMode(..., swizzle=0)`). SDK поддерживает swizzle=1 для лучшей работы кеша; можно свизлить при загрузке.
  - Код: `src/graphics.c:476`, `src/png.c:130`
  - SDK: `pspsdk-master/src/gu/pspgu.h:1341`

## Мусор / неиспользуемый код

- В `src` лежат артефакты сборки: `*.o`, `*.d`, `*-safeBackup-*.o`. Это мусор и должно быть удалено/добавлено в `.gitignore`.
  - Примеры: `src/game.o`, `src/main.d`, `src/game-user-VMware-Virtual-Platform-safeBackup-0001.o`

- `font16` нигде не используется (нет ссылок в `src`), но файлы присутствуют.
  - Файлы: `src/font16.c`, `src/font16.h`

## Примечания / legacy

- Есть явные legacy‑значения цветов в логике уровней (не ошибка, но «магия»).
  - `src/level.h:40`, `src/level.c:266`

- TODO в микшере звука про equal‑power коэффициенты (не критично, но можно улучшить звучание).
  - `src/sound.c:381`

---

# Все float-значения в коде

Ниже перечислены все места, где встречается `float` (по `rg -n "\bfloat\b" src`).

## src/game.c
- `src/game.c:51` — `static void draw_bonus_bar(float x, float y, int bonus_value) {` [px]
- `src/game.c:53` — `    const float frame_width = 62.0f;` [px]
- `src/game.c:54` — `    const float frame_height = 10.0f;` [px]
- `src/game.c:55` — `    const float bar_width = 60.0f;` [px]
- `src/game.c:56` — `    const float bar_height = 8.0f;` [px]
- `src/game.c:66` — `        float current_bar_width = bar_width * (float)bonus_value / 300.0f;` [px]
- `src/game.c:296` — `            float text_y = 136 + 70;  // центр экрана + отступ = 206px` [px]
- `src/game.c:300` — `            float text_width = graphics_measure_text(press_start_text, 9);` [px]
- `src/game.c:301` — `            float text_x = (SCREEN_WIDTH - text_width) / 2.0f;` [px]
- `src/game.c:454` — `            const float hudStartY = SCREEN_HEIGHT - HUD_HEIGHT;` [px]
- `src/game.c:455` — `            float separator_y = hudStartY;` [px]
- `src/game.c:456` — `            float hud_blue_y = separator_y + 1.0f;` [px]
- `src/game.c:459` — `            graphics_draw_rect(0.0f, separator_y, (float)SCREEN_WIDTH, 1.0f, COLOR_WHITE_ABGR);` [px]
- `src/game.c:462` — `            graphics_draw_rect(0.0f, hud_blue_y, (float)SCREEN_WIDTH, 16.0f, HUD_COLOUR);` [px]
- `src/game.c:515` — `            float score_x = (SCREEN_WIDTH - graphics_measure_text(score_buffer, 9)) / 2.0f;` [px]
- `src/game.c:516` — `            float score_y = hudStartY + 5.0f;  // Поднял на 1 пиксель` [px]
- `src/game.c:521` — `                float text_width = graphics_measure_text(score_buffer, 9);` [px]
- `src/game.c:522` — `                float bonus_x = score_x + text_width + 10.0f + 30.0f; // После счета с отступом 10px, сдвиг на 30px вправо` [px]
- `src/game.c:523` — `                float bonus_y = hudStartY + 4.0f;  // На 1 пиксель ниже (было 3.0f)` [px]

## src/graphics.c
- `src/graphics.c:40` — `    float u, v;` [px]
- `src/graphics.c:41` — `    float x, y, z;` [px]
- `src/graphics.c:115` — `void graphics_draw_rect(float x, float y, float w, float h, u32 color) {` [px]
- `src/graphics.c:233` — `void graphics_draw_text(float x, float y, const char* text, u32 color, int font_height) {` [px]
- `src/graphics.c:235` — `    float cur_x = x;` [px]
- `src/graphics.c:294` — `float graphics_measure_text(const char* text, int font_height) {` [px]
- `src/graphics.c:296` — `    float width = 0.0f;` [px]
- `src/graphics.c:338` — `void graphics_draw_number(float x, float y, int number, u32 color) {` [px]
- `src/graphics.c:366` — `float graphics_measure_number(int number) {` [px]
- `src/graphics.c:370` — `    float width = 0;` [px]
- `src/graphics.c:488` — `void graphics_batch_sprite(float u1, float v1, float u2, float v2, ` [px]
- `src/graphics.c:489` — `                          float x, float y, float w, float h) {` [px]
- `src/graphics.c:533` — `void graphics_draw_button_x(float cx, float cy, float radius) {` [px]
- `src/graphics.c:542` — `    float start_x = cx - 6.0f;  // 13/2 = 6.5, округляем до 6` [px]
- `src/graphics.c:543` — `    float start_y = cy - 6.0f;` [px]

## src/graphics.h
- `src/graphics.h:81` — `void graphics_draw_rect(float x, float y, float w, float h, u32 color);` [px]
- `src/graphics.h:91` — `void graphics_draw_text(float x, float y, const char* text, u32 color, int font_height);` [px]
- `src/graphics.h:99` — `float graphics_measure_text(const char* text, int font_height);` [px]
- `src/graphics.h:108` — `void graphics_draw_number(float x, float y, int number, u32 color);` [px]
- `src/graphics.h:115` — `float graphics_measure_number(int number);` [px]
- `src/graphics.h:123` — `void graphics_draw_button_x(float x, float y, float radius);` [px]
- `src/graphics.h:148` — `void graphics_batch_sprite(float u1, float v1, float u2, float v2, ` [px]
- `src/graphics.h:149` — `                          float x, float y, float w, float h); // Добавить спрайт в batch` [px]

## src/level.c
- `src/level.c:51` — `typedef struct { int sprite_idx; float x, y; int transform; } hoop_fg_item_t;` [px]
- `src/level.c:56` — `static inline void hoop_fg_push(int sprite_idx, float x, float y, int transform){` [px]
- `src/level.c:253` — `static void render_exit_tile(int tile_id, float destX, float destY, int worldTileX, int worldTileY) {` [px]
- `src/level.c:271` — `            float area_width = 2.0f * TILE_SIZE;   // 24 пикселя (TILE_SIZE=12)` [px]
- `src/level.c:272` — `            float area_height = 2.0f * TILE_SIZE;  // 24 пикселя` [px]
- `src/level.c:280` — `            graphics_draw_rect(destX + EXIT_STRIPE_1_X, destY, (float)EXIT_STRIPE_1_WIDTH, area_height, first_stripe);` [px]
- `src/level.c:281` — `            graphics_draw_rect(destX + EXIT_STRIPE_2_X, destY, (float)EXIT_STRIPE_2_WIDTH, area_height, light_stripe);` [px]
- `src/level.c:282` — `            graphics_draw_rect(destX + EXIT_STRIPE_3_X, destY, (float)EXIT_STRIPE_3_WIDTH, area_height, dark_stripe);` [px]
- `src/level.c:283` — `            graphics_draw_rect(destX + EXIT_STRIPE_4_X, destY, (float)EXIT_STRIPE_4_WIDTH, area_height, fourth_stripe);` [px]
- `src/level.c:299` — `            float doorX = destX;` [px]
- `src/level.c:300` — `            float doorY = destY - animationOffset;` [px]
- `src/level.c:303` — `            float areaTop = destY;` [px]
- `src/level.c:308` — `                float clipOffset = areaTop - doorY;` [px]
- `src/level.c:310` — `                    float visibleHeight = TILE_SIZE - clipOffset;` [px]
- `src/level.c:346` — `        graphics_draw_rect(destX, destY, (float)TILE_SIZE, (float)TILE_SIZE, WATER_COLOUR); // темный фон` [px]
- `src/level.c:349` — `        graphics_draw_rect(destX, destY, (float)TILE_SIZE, (float)TILE_SIZE, 0xFF888888);` [px]
- `src/level.c:354` — `static void render_moving_spikes_tile(int tileX, int tileY, float destX, float destY) {` [px]
- `src/level.c:367` — `        graphics_draw_rect(destX, destY, (float)TILE_SIZE, (float)TILE_SIZE, bg_color);` [px]
- `src/level.c:381` — `    graphics_draw_rect(destX, destY, (float)TILE_SIZE, (float)TILE_SIZE, bg_color);` [px]
- `src/level.c:389` — `    float offsetX = (float)obj->offset[0] - (float)(relTileX * TILE_SIZE);` [px]
- `src/level.c:390` — `    float offsetY = (float)obj->offset[1] - (float)(relTileY * TILE_SIZE);` [px]
- `src/level.c:404` — `                float spriteX = destX + offsetX + (float)(dx * TILE_SIZE);` [px]
- `src/level.c:405` — `                float spriteY = destY + offsetY + (float)(dy * TILE_SIZE);` [px]
- `src/level.c:432` — `static void render_hoop_tile(const TileMeta* t, float destX, float destY, int flags, int tileID) {` [px]
- `src/level.c:440` — `    graphics_draw_rect(destX, destY, (float)TILE_SIZE, (float)TILE_SIZE, bg_color);` [px]
- `src/level.c:516` — `            float screenX = (float)(x * TILE_SIZE - cameraX);` [px]
- `src/level.c:517` — `            float screenY = (float)(y * TILE_SIZE - cameraY);` [px]
- `src/level.c:527` — `                graphics_draw_rect(screenX, screenY, (float)TILE_SIZE, (float)TILE_SIZE, bg_color);` [px]
- `src/level.c:544` — `                    graphics_draw_rect(screenX, screenY, (float)TILE_SIZE, (float)TILE_SIZE, WATER_COLOUR); // фон воды` [px]
- `src/level.c:602` — `                        graphics_draw_rect(screenX, screenY, (float)TILE_SIZE, (float)TILE_SIZE, 0xFF444444);` [px]
- `src/level.c:607` — `                graphics_draw_rect(screenX, screenY, (float)TILE_SIZE, (float)TILE_SIZE, 0xFF444444);` [px]

## src/menu.c
- `src/menu.c:33` — `static void draw_wrapped_text(float x, float y, const char* text, u32 color, float max_width, float line_height, int font_height) {` [px]
- `src/menu.c:38` — `    float current_y = y;` [px]
- `src/menu.c:52` — `            float width = graphics_measure_text(temp_line, font_height);` [px]
- `src/menu.c:165` — `const float center_x = (float)(SCREEN_WIDTH / 2);` [px]
- `src/menu.c:166` — `    const float title_y  = MAIN_MENU_TITLE_Y;` [px]
- `src/menu.c:173` — `        float w = graphics_measure_text(title, font_height);` [px]
- `src/menu.c:174` — `        float x = center_x - w * 0.5f;` [px]
- `src/menu.c:179` — `    struct Item { const char* label; float y; u32 color_unselected; };` [px]
- `src/menu.c:200` — `        float w = graphics_measure_text(items[i].label, font_height);` [px]
- `src/menu.c:201` — `        float x = center_x - w * 0.5f;` [px]
- `src/menu.c:202` — `        float y = items[i].y;` [px]
- `src/menu.c:206` — `            const float padding_x = MAIN_MENU_PADDING_X;` [px]
- `src/menu.c:207` — `            const float padding_y = MAIN_MENU_PADDING_Y;` [px]
- `src/menu.c:224` — `    graphics_draw_rect(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, BACKGROUND_COLOUR);` [px]
- `src/menu.c:227` — `    float panel_x = (SCREEN_WIDTH - 240) / 2.0f;  // 120` [px]
- `src/menu.c:228` — `    float panel_y = (SCREEN_HEIGHT - 128) / 2.0f;  // 72` [px]
- `src/menu.c:231` — `    float center_x = (float)SCREEN_WIDTH / 2.0f;` [px]
- `src/menu.c:232` — `    float text_y = panel_y + 5.0f;  // Отступ от верха панели для шапки` [px]
- `src/menu.c:236` — `    float w = graphics_measure_text(title, 23);` [px]
- `src/menu.c:241` — `    float max_width = 240.0f - 40.0f; // Ширина панели минус отступы по 20px с каждой стороны` [px]
- `src/menu.c:242` — `    float line_height = 14.0f; // Уменьшенная высота строки для помещения в панель` [px]
- `src/menu.c:253` — `    float page_w = graphics_measure_text(page_info, 9);` [px]
- `src/menu.c:309` — `    const float center_x = (float)(SCREEN_WIDTH / 2);` [px]
- `src/menu.c:316` — `        float w = graphics_measure_text(title, 23);` [px]
- `src/menu.c:317` — `        float x = center_x - w * 0.5f;` [px]
- `src/menu.c:338` — `                graphics_draw_rect((float)(x - 5), (float)(y - 5), (float)cell_width, (float)cell_height, COLOR_SELECTION_BG);` [px]
- `src/menu.c:346` — `            graphics_draw_text((float)x, (float)y, level_text, color, 23);` [px]
- `src/menu.c:355` — `        graphics_draw_rect((float)(x11 - 5), (float)(y11 - 5), (float)cell_width, (float)cell_height, COLOR_SELECTION_BG);` [px]
- `src/menu.c:361` — `    graphics_draw_text((float)x11, (float)y11, level11_text, color11, 23);` [px]
- `src/menu.c:381` — `    graphics_draw_rect(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, BACKGROUND_COLOUR);` [px]
- `src/menu.c:384` — `    float panel_x = (SCREEN_WIDTH - 240) / 2.0f;  // 120` [px]
- `src/menu.c:385` — `    float panel_y = (SCREEN_HEIGHT - 128) / 2.0f;  // 72` [px]
- `src/menu.c:388` — `    float center_x = (float)SCREEN_WIDTH / 2.0f;` [px]
- `src/menu.c:389` — `    float text_y = panel_y + 5.0f;  // Отступ от верха панели для шапки` [px]
- `src/menu.c:394` — `        float w = graphics_measure_text(title, 23);` [px]
- `src/menu.c:407` — `        float w = graphics_measure_number(save->best_score);` [px]
- `src/menu.c:415` — `        float w = graphics_measure_text(continue_text, 9);` [px]
- `src/menu.c:418` — `        float total_width = 12.0f + 4.0f + w;` [px]
- `src/menu.c:419` — `        float start_x = center_x - total_width * 0.5f;` [px]
- `src/menu.c:441` — `    graphics_draw_rect(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, BACKGROUND_COLOUR);` [px]
- `src/menu.c:444` — `    float panel_x = (SCREEN_WIDTH - 240) / 2.0f;  // 120` [px]
- `src/menu.c:445` — `    float panel_y = (SCREEN_HEIGHT - 128) / 2.0f;  // 72` [px]
- `src/menu.c:448` — `    float center_x = (float)SCREEN_WIDTH / 2.0f;  // 240` [px]
- `src/menu.c:449` — `    float text_y = panel_y + 5.0f;  // Отступ от верха панели для шапки` [px]
- `src/menu.c:454` — `        float w = graphics_measure_text(title, 23);` [px]
- `src/menu.c:462` — `        float w = graphics_measure_text(new_record, 9);` [px]
- `src/menu.c:472` — `        float w = graphics_measure_number(g_game.score);` [px]
- `src/menu.c:480` — `        float w = graphics_measure_text(ok_text, 9);` [px]
- `src/menu.c:483` — `        float total_width = 12.0f + 4.0f + w;` [px]
- `src/menu.c:484` — `        float start_x = center_x - total_width * 0.5f;` [px]
- `src/menu.c:534` — `    graphics_draw_rect(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, BACKGROUND_COLOUR);` [px]
- `src/menu.c:537` — `    float panel_x = (SCREEN_WIDTH - 240) / 2.0f;  // 120` [px]
- `src/menu.c:538` — `    float panel_y = (SCREEN_HEIGHT - 128) / 2.0f;  // 72` [px]
- `src/menu.c:541` — `    float center_x = (float)SCREEN_WIDTH / 2.0f;  // 240` [px]
- `src/menu.c:542` — `    float text_y = panel_y + 5.0f;  // Отступ от верха панели для шапки` [px]
- `src/menu.c:547` — `        float w = graphics_measure_text(title, 23);` [px]
- `src/menu.c:558` — `        float w = graphics_measure_text(title, 9);` [px]
- `src/menu.c:565` — `        float w = graphics_measure_number(g_game.score);` [px]
- `src/menu.c:573` — `        float w = graphics_measure_text(continue_text, 9);` [px]
- `src/menu.c:576` — `        float total_width = 12.0f + 4.0f + w;` [px]
- `src/menu.c:577` — `        float start_x = center_x - total_width * 0.5f;` [px]

## src/png.c
- `src/png.c:27` — `static inline float clamp_bounds(float value, float min, float max) {` [px]
- `src/png.c:94` — `    float u, v;` [px]
- `src/png.c:95` — `    float x, y, z;` [px]
- `src/png.c:272` — `    rect.u = (float)x / (float)tex->width;`
- `src/png.c:273` — `    rect.v = (float)y / (float)tex->height;`
- `src/png.c:274` — `    rect.width  = (float)w / (float)tex->width;`
- `src/png.c:275` — `    rect.height = (float)h / (float)tex->height;`
- `src/png.c:285` — `    float u1 = sprite->u * (float)tex->width;` [px]
- `src/png.c:286` — `    float v1 = sprite->v * (float)tex->height;` [px]
- `src/png.c:287` — `    float u2 = (sprite->u + sprite->width) * (float)tex->width;` [px]
- `src/png.c:288` — `    float v2 = (sprite->v + sprite->height) * (float)tex->height;` [px]
- `src/png.c:292` — `    graphics_batch_sprite(u1, v1, u2, v2, (float)x, (float)y, (float)w, (float)h);  // Добавляем в batch` [px]
- `src/png.c:307` — `                         float u_tl, float v_tl,` [px]
- `src/png.c:308` — `                         float u_tr, float v_tr,` [px]
- `src/png.c:309` — `                         float u_bl, float v_bl,` [px]
- `src/png.c:310` — `                         float u_br, float v_br,` [px]
- `src/png.c:311` — `                         float x, float y, float w, float h)` [px]
- `src/png.c:320` — `    float tw = (float)tex->width, th = (float)tex->height;` [px]
- `src/png.c:363` — `    const float u1 = sprite->u * (float)tex->width;` [px]
- `src/png.c:364` — `    const float v1 = sprite->v * (float)tex->height;` [px]
- `src/png.c:365` — `    const float u2 = (sprite->u + sprite->width)  * (float)tex->width;` [px]
- `src/png.c:366` — `    const float v2 = (sprite->v + sprite->height) * (float)tex->height;` [px]
- `src/png.c:369` — `    float U[4] = { u1, u2, u1, u2 };` [px]
- `src/png.c:370` — `    float V[4] = { v1, v1, v2, v2 };` [px]
- `src/png.c:411` — `    const float tl_u = U[idx[0]], tl_v = V[idx[0]];` [px]
- `src/png.c:412` — `    const float tr_u = U[idx[1]], tr_v = V[idx[1]];` [px]
- `src/png.c:413` — `    const float bl_u = U[idx[2]], bl_v = V[idx[2]];` [px]
- `src/png.c:414` — `    const float br_u = U[idx[3]], br_v = V[idx[3]];` [px]
- `src/png.c:417` — `                        (float)x, (float)y, (float)w, (float)h);` [px]

## src/png.h
- `src/png.h:19` — `    float u, v;          // top-left UV (0..1)`
- `src/png.h:20` — `    float width, height; // UV size (0..1)`
- `src/png.h:66` — `                         float u_tl, float v_tl,` [px]
- `src/png.h:67` — `                         float u_tr, float v_tr,` [px]
- `src/png.h:68` — `                         float u_bl, float v_bl,` [px]
- `src/png.h:69` — `                         float u_br, float v_br,` [px]
- `src/png.h:70` — `                         float x, float y, float w, float h);` [px]

## src/sound.c
- `src/sound.c:33` — `        float angle = 2.0f * (float)M_PI * (float)i / (float)WAVETABLE_SIZE;`
- `src/sound.c:34` — `        float sine_value = sinf(angle);`
- `src/sound.c:43` — `static uint32_t frequency_to_phase_inc(float frequency, int sample_rate) {`
- `src/sound.c:228` — `float ott_tone_to_frequency(int tone, int scale) {`
- `src/sound.c:233` — `    return (float)(tone_freqs[tone] * (1 << scale));`
- `src/sound.c:237` — `float ott_length_to_duration(int length, int bpm) {`
- `src/sound.c:247` — `    float beats_per_minute = (float)bpm;`
- `src/sound.c:248` — `    float seconds_per_beat = 60.0f / beats_per_minute;`
- `src/sound.c:249` — `    float note_fraction = 4.0f / (float)(1 << length); // whole=4, half=2, quarter=1, eighth=0.5, etc`
- `src/sound.c:300` — `static short generate_player_sample(struct ott_player_t *player, float sample_length) {`
- `src/sound.c:360` — `    const float sample_length = 1.0f / (float)PSP_SR; // PSP sample rate`

## src/sound.h
- `src/sound.h:40` — `    float note_time;`
- `src/sound.h:41` — `    float note_duration;`
- `src/sound.h:42` — `    float frequency;`
- `src/sound.h:57` — `float ott_tone_to_frequency(int tone, int scale);`
- `src/sound.h:58` — `float ott_length_to_duration(int length, int bpm);`
