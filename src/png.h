#ifndef PNG_H
#define PNG_H

#include <pspgu.h>

// ТОЧНАЯ КОПИЯ вашей структуры texture_t
typedef struct {
    void* data;         // Texture data
    int width;          // Texture width (atlas width, pixels)
    int height;         // Texture height (atlas height, pixels)
    int actual_width;   // Actual image width
    int actual_height;  // Actual image height
    int format;         // GU pixel format (GU_PSM_8888, etc.)
    int is_vram;        // 1 if texture is in VRAM, 0 if in RAM
} texture_t;

// ТОЧНАЯ КОПИЯ вашей структуры sprite_rect_t
typedef struct {
    float u, v;          // top-left UV (0..1)
    float width, height; // UV size (0..1)
} sprite_rect_t;

/**
 * Load PNG file into GU texture (VRAM)
 */
texture_t* png_load_texture_vram(const char* path);

/**
 * Create sprite rectangle for atlas texture
 */
sprite_rect_t png_create_sprite_rect(texture_t* tex, int x, int y, int w, int h);

/**
 * Draw sprite from texture atlas
 * Ожидается: GU_TEXTURE_2D включён, GU_TCC_RGBA, BLEND включён; при необходимости — GU_ALPHA_TEST (A>0).
 * Рекомендуется: GU_ALPHA_TEST с порогом >0 для PNG с прозрачностью.
 */
void png_draw_sprite(texture_t* tex, sprite_rect_t* sprite, int x, int y, int w, int h);

/**
 * Free texture memory
 */
void png_free_texture(texture_t* tex);



typedef enum {
    PNG_TRANSFORM_IDENTITY = 0,
    PNG_TRANSFORM_ROT_90,
    PNG_TRANSFORM_ROT_180,
    PNG_TRANSFORM_ROT_270,
    PNG_TRANSFORM_FLIP_X,
    PNG_TRANSFORM_FLIP_Y,
    // Составные трансформации для Java-совместимости
    PNG_TRANSFORM_ROT_270_FLIP_X,    // ROT_270 + FLIP_X (для Java tileImages[35])
    PNG_TRANSFORM_ROT_270_FLIP_Y,    // ROT_270 + FLIP_Y (для Java tileImages[34])
    PNG_TRANSFORM_ROT_270_FLIP_XY    // ROT_270 + FLIP_X + FLIP_Y
} png_transform_t;

/**
 * Draw sprite with explicit 4-corner UVs (needed for rotation/mirroring).
 * Ожидается: GU_TEXTURE_2D включён, GU_TCC_RGBA, BLEND включён; при необходимости — GU_ALPHA_TEST (A>0).
 * Рекомендуется: GU_ALPHA_TEST с порогом >0 для PNG с прозрачностью.
 */
void png_draw_sprite_uv4(texture_t* tex,
                         float u_tl, float v_tl,
                         float u_tr, float v_tr,
                         float u_bl, float v_bl,
                         float u_br, float v_br,
                         int x, int y, int w, int h);

/**
 * Draw sprite with a transform (rotation/mirror) applied to the given sprite rect.
 * Ожидается: GU_TEXTURE_2D включён, GU_TCC_RGBA, BLEND включён; при необходимости — GU_ALPHA_TEST (A>0).
 * Рекомендуется: GU_ALPHA_TEST с порогом >0 для PNG с прозрачностью.
 */
void png_draw_sprite_transform(texture_t* tex, sprite_rect_t* sprite,
                           int x, int y, int w, int h,
                           png_transform_t transform);


#endif // PNG_H
