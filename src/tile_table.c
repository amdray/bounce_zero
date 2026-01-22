#include "tile_table.h"
#include "level.h"


__attribute__((aligned(64)))
static const TileMeta TILE_DB[55] = {
    /* case 00 - EMPTY_SPACE - пустой тайл, фон заливается цветом в зависимости от флага */
    { ORIENT_NONE, 0, 255, TF_NONE, RENDER_NORMAL},

    /* case 01 - BRICK_RED tileImages[0] = extractImage(image, 1, 0) = атлас[1] */
    { ORIENT_NONE, 0, 1, TF_NONE, RENDER_NORMAL},

    /* case 02 - BRICK_BLUE tileImages[1] = extractImage(image, 1, 2) = атлас[9] */
    { ORIENT_NONE, 0, 9, TF_NONE, RENDER_NORMAL},

    /* case 03 - Шипы направлены вверх (тонкие горизонтально, i+=4, k-=4) tileImages[6] : tileImages[2] - оба из атласа (0,3) */
    { ORIENT_SPIKE_THIN_HORIZ, 0, 12, TF_NONE, RENDER_NORMAL},

    /* case 04 - Шипы направлены влево (тонкие вертикально, j+=4, m-=4) tileImages[9] : tileImages[5] = manipulateImage(базовый, 5) = ROT_270 */
    { ORIENT_SPIKE_THIN_VERT, 0, 12, TF_ROT_270, RENDER_NORMAL},

    /* case 05 - Шипы направлены вниз (тонкие горизонтально, i+=4, k-=4) tileImages[7] : tileImages[3] = manipulateImage(базовый, 1) = FLIP_Y */
    { ORIENT_SPIKE_THIN_HORIZ, 0, 12, TF_FLIP_Y, RENDER_NORMAL},

    /* case 06 - Шипы направлены вправо (тонкие вертикально, j+=4, m-=4) tileImages[8] : tileImages[4] = manipulateImage(базовый, 3) = ROT_90 */
    { ORIENT_SPIKE_THIN_VERT, 0, 12, TF_ROT_90, RENDER_NORMAL},

    /* case 07 RESPAWN_GEM кристал чекпоинта, при сборке переключает точку старта на себя - tileImages[10] = extractImage(image, 0, 4) = атлас[16] */
    { ORIENT_NONE, 0, 16, TF_NONE, RENDER_NORMAL},

    /* case 08 RESPAWN_INDICATOR стрелка вместо кристала чекпоинта - tileImages[11] = extractImage(image, 3, 4) = атлас[19] - ПРОХОДИМЫЙ */
    { ORIENT_NONE, 0, 19, TF_NONE, RENDER_NORMAL},

    /* case 09 EXIT - составной тайл из tileImages[12] = createExitImage(атлас[14]) */
    { ORIENT_NONE, 0, 14, TF_NONE, RENDER_NORMAL},

    /* case 10 MOVING_SPIKES - составной из tileImages[46] = атлас[13] (специальная коллизия 24x24) */
    { ORIENT_NONE, 0, 13, TF_NONE, RENDER_COMPOSITE},

    { ORIENT_NONE, 0, 255, TF_NONE, RENDER_NORMAL},    // case 11 - вырезанный case
    { ORIENT_NONE, 0, 255, TF_NONE, RENDER_NORMAL},    // case 12 - вырезанный case

    { ORIENT_VERT_TOP, 0, 21, TF_NONE, RENDER_HOOP},     /* case 13 RING - ID_HOOP_ACTIVE_VERT_TOP = 13 */
    { ORIENT_VERT_BOTTOM, 0, 21, TF_NONE, RENDER_HOOP},     /* case 14 RING - ID_HOOP_ACTIVE_VERT_BOTTOM = 14 */

    { ORIENT_HORIZ_LEFT, 0, 21, TF_NONE, RENDER_HOOP},    /* case 15 RING - ID_HOOP_ACTIVE_HORIZ_LEFT = 15 */
    { ORIENT_HORIZ_RIGHT, 0, 21, TF_NONE, RENDER_HOOP},     /* case 16 RING - ID_HOOP_ACTIVE_HORIZ_RIGHT = 16 */

    { ORIENT_VERT_TOP, 0, 23, TF_NONE, RENDER_HOOP},     /* case 17 RING - ID_HOOP_INACTIVE_VERT_TOP = 17 */ 
    { ORIENT_VERT_BOTTOM, 0, 23, TF_NONE, RENDER_HOOP},    /* case 18 RING - ID_HOOP_INACTIVE_VERT_BOTTOM = 18 */

    { ORIENT_HORIZ_LEFT, 0, 23, TF_NONE, RENDER_HOOP},    /* case 19 RING - ID_HOOP_INACTIVE_HORIZ_LEFT = 19 */
    { ORIENT_HORIZ_RIGHT, 0, 23, TF_NONE, RENDER_HOOP},    /* case 20 RING - ID_HOOP_INACTIVE_HORIZ_RIGHT = 20 */

    { ORIENT_VERT_TOP, 0, 20, TF_NONE, RENDER_HOOP},     /* case 21 RING - ID_LARGE_HOOP_ACTIVE_VERT_TOP = 21 */
    { ORIENT_VERT_BOTTOM, 0, 20, TF_NONE, RENDER_HOOP},     /* case 22 RING - ID_LARGE_HOOP_ACTIVE_VERT_BOTTOM = 22 */


    { ORIENT_HORIZ_LEFT, 0, 20, TF_NONE, RENDER_HOOP},    /* case 23 RING - ID_LARGE_HOOP_ACTIVE_HORIZ_LEFT = 23 */
    { ORIENT_HORIZ_RIGHT, 0, 20, TF_NONE, RENDER_HOOP},    /* case 24 RING - ID_LARGE_HOOP_ACTIVE_HORIZ_RIGHT = 24 */


    { ORIENT_VERT_TOP, 0, 22, TF_NONE, RENDER_HOOP},    /* case 25 RING - ID_LARGE_HOOP_INACTIVE_VERT_TOP = 25 */
    { ORIENT_VERT_BOTTOM, 0, 22, TF_NONE, RENDER_HOOP},    /* 26 RING - ID_LARGE_HOOP_INACTIVE_VERT_BOTTOM = 26 */


    { ORIENT_HORIZ_LEFT, 0, 22, TF_NONE, RENDER_HOOP},    /* 27 RING - ID_LARGE_HOOP_INACTIVE_HORIZ_LEFT = 27 */
    { ORIENT_HORIZ_RIGHT, 0, 22, TF_NONE, RENDER_HOOP},       /* 28 RING - ID_LARGE_HOOP_INACTIVE_HORIZ_RIGHT = 28 */
    
    /* case 29 - Прозрачный шар, добавляет жизни, исчезает: tileImages[45] = extractImage(image, 3, 3) = атлас[15] - EXTRA LIFE (хрустальный шар) */
    { ORIENT_NONE, 0, 15, TF_NONE, RENDER_NORMAL},

    /* case 30 - Рампа пол: ◣ (поворот на 180°)        Java: bool ? tileImages[61] : tileImages[57] = manipulateImage(базовый, 4) = ROT_180 */
    { ORIENT_TL, 0, 0, TF_ROT_180, RENDER_NORMAL},

    /* case 31 - Рампа пол: ◤ (поворот на 90°)       Java: bool ? tileImages[60] : tileImages[56] = manipulateImage(базовый, 3) = ROT_90 */
    { ORIENT_TR, 0, 0, TF_ROT_90, RENDER_NORMAL},

    /* case 32 - Рампа пол: ◥ (базовый спрайт) Java: bool ? tileImages[59] : tileImages[55] = extractImageBG(базовый, 0, 0) */
    { ORIENT_BR, 0, 0, TF_NONE, RENDER_NORMAL},

    /* case 33 - Рампа пол: ◢ (поворот на 270°) Java: bool ? tileImages[62] : tileImages[58] = manipulateImage(базовый, 5) = ROT_270 */
    { ORIENT_BL, 0, 0, TF_ROT_270, RENDER_NORMAL},

    /* case 34 - Резиновая рампа: ◣ (поворот на 180°) Java: tileImages[65] = manipulateImage(базовый, 4) = ROT_180 */
    { ORIENT_TL, 0, 8, TF_ROT_180, RENDER_NORMAL},

    /* case 35 - Резиновая рампа: ◤ (поворот на 90°) Java: tileImages[64] = manipulateImage(базовый, 3) = ROT_90 */
    { ORIENT_TR, 0, 8, TF_ROT_90, RENDER_NORMAL},

    /* case 36 - Резиновая рампа: ◥ (базовый спрайт) Java: tileImages[63] = extractImage(image, 0, 2) */
    { ORIENT_BR, 0, 8, TF_NONE, RENDER_NORMAL},

    /* case 37 - Резиновая рампа:  ◢ (поворот на 270°) Java: tileImages[66] = manipulateImage(базовый, 5) = ROT_270 */
    { ORIENT_BL, 0, 8, TF_ROT_270, RENDER_NORMAL},

    /* 38 - ID_SPEED = 38 - бонус скорости */
    { ORIENT_NONE, 0, 5, TF_FLIP_X, RENDER_NORMAL}, // ID_SPEED

    /* 39-42 DEFLATOR - tileImages[50] = extractImage(image, 3, 1) = атлас[7] */
    { ORIENT_NONE, 0, 7, TF_NONE, RENDER_NORMAL},       // ID_DEFLATOR_FLOOR
    { ORIENT_NONE, 0, 7, TF_ROT_90, RENDER_NORMAL},     // ID_DEFLATOR_LEFT_WALL
    { ORIENT_NONE, 0, 7, TF_ROT_180, RENDER_NORMAL},    // ID_DEFLATOR_CEILING
    { ORIENT_NONE, 0, 7, TF_ROT_270, RENDER_NORMAL},    // ID_DEFLATOR_RIGHT_WALL

    /* 43-46 INFLATOR - tileImages[51] = extractImage(image, 2, 4) = атлас[18] */
    { ORIENT_NONE, 0, 18, TF_NONE, RENDER_NORMAL},      // ID_INFLATOR_FLOOR
    { ORIENT_NONE, 0, 18, TF_ROT_90, RENDER_NORMAL},    // ID_INFLATOR_LEFT_WALL
    { ORIENT_NONE, 0, 18, TF_ROT_180, RENDER_NORMAL},   // ID_INFLATOR_CEILING
    { ORIENT_NONE, 0, 18, TF_ROT_270, RENDER_NORMAL},   // ID_INFLATOR_RIGHT_WALL

    /* 47-50 GRAVITY - ID_GRAVITY_FLOOR/LEFT_WALL/CEILING/RIGHT_WALL */
    { ORIENT_NONE, 0, 11, TF_NONE, RENDER_NORMAL},     // ID_GRAVITY_FLOOR
    { ORIENT_NONE, 0, 11, TF_ROT_90, RENDER_NORMAL},   // ID_GRAVITY_LEFT_WALL
    { ORIENT_NONE, 0, 11, TF_ROT_180, RENDER_NORMAL},  // ID_GRAVITY_CEILING
    { ORIENT_NONE, 0, 11, TF_ROT_270, RENDER_NORMAL},  // ID_GRAVITY_RIGHT_WALL

    /* 51-54 JUMP - ID_JUMP_FLOOR/LEFT_WALL/CEILING/RIGHT_WALL */
    { ORIENT_NONE, 0, 10, TF_NONE, RENDER_NORMAL},     // ID_JUMP_FLOOR 
    { ORIENT_NONE, 0, 10, TF_ROT_270, RENDER_NORMAL},  // ID_JUMP_LEFT_WALL
    { ORIENT_NONE, 0, 10, TF_ROT_180, RENDER_NORMAL},  // ID_JUMP_CEILING
    { ORIENT_NONE, 0, 10, TF_ROT_90, RENDER_NORMAL},   // ID_JUMP_RIGHT_WALL

};

const TileMeta* tile_meta_db(void) {
    return TILE_DB;
}

uint32_t tile_meta_count(void) {
    return 55;
}


