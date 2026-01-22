// physics.c - Портированная физика из Ball.java (с подводной физикой)
#include "types.h"
#include "level.h"
#include "tile_table.h"
#include "game.h"        // Для событийного API
#include "sound.h"       // Для звуковых эффектов
#include <stdlib.h>
#include <assert.h>

// Битовые маски для пиксельных коллизий
// КРИТИЧНО: Маски коллизий привязаны к размеру тайла 12x12 пикселя из оригинальной Java-версии Bounce
_Static_assert(TILE_SIZE == 12, "TILE_SIZE должен быть 12: маски коллизий в level_masks.inc жестко привязаны к размеру 12x12 пикселя из оригинальной игры Bounce. Для изменения размера потребуется регенерация всех масок коллизий.");
#include "level_masks.inc"

// Forward declarations
static bool collisionDetection(Player* p, int testX, int testY);
static bool testTile(Player* p, int tileY, int tileX, bool canMove);
static bool squareCollide(Player* p, int tileRow, int tileCol);
static bool triangleCollide(Player* p, int tileRow, int tileCol, int tileID);
static bool thinCollide(Player* p, int tileRow, int tileCol, int tileID);
 

// Helper для получения тайла по центру мяча (Java-совместимая система координат)
// Везде xPos/yPos = центр мяча, как в оригинале
static inline void player_center_tile(const Player* p, int* tileX, int* tileY) {
    *tileX = p->xPos / TILE_SIZE;
    *tileY = p->yPos / TILE_SIZE;
}
static bool edgeCollide(Player* p, int tileRow, int tileCol, int tileID);
static void redirectBall(Player* p, int tileID);
static void clip_to_tile_bounds(int relPos, int ballSize, int* startBound, int* endBound);
static void clamp_speed(Player* p);


// Размер области движущихся шипов
#define MOVING_SPIKE_PX (2 * TILE_SIZE)


// Helper для вычисления границ пересечения мяча с тайлом (устраняет дублирование)
// Используется в squareCollide и triangleCollide для одинаковой логики clipping
static void clip_to_tile_bounds(int relPos, int ballSize, int* startBound, int* endBound) {
    if (relPos >= 0) {
        *startBound = relPos;
        *endBound = TILE_SIZE;
    } else {
        *startBound = 0;
        *endBound = ballSize + relPos;
    }
}

// Ограничение скорости мяча (аналог Java Ball.java:971-977)
// КРИТИЧНО: без этого мяч может улететь за границы экрана и застрять
// Применяется после всех вычислений скорости, включая бонусы
static void clamp_speed(Player* p) {
    if (p->ySpeed < -MAX_TOTAL_SPEED) p->ySpeed = -MAX_TOTAL_SPEED;
    else if (p->ySpeed > MAX_TOTAL_SPEED) p->ySpeed = MAX_TOTAL_SPEED;
    if (p->xSpeed < -MAX_TOTAL_SPEED) p->xSpeed = -MAX_TOTAL_SPEED;
    else if (p->xSpeed > MAX_TOTAL_SPEED) p->xSpeed = MAX_TOTAL_SPEED;
}

// Утилита для rect коллизий (эквивалент TileCanvas.rectCollide)
static bool rectCollide(int x1, int y1, int x2, int y2, int rx1, int ry1, int rx2, int ry2);

// Константы перенесены в types.h для централизации

// Инициализация игрока
void player_init(Player* p, int x, int y, BallSizeState sizeState) {
    p->xPos = x;
    p->yPos = y;
    p->globalBallX = 0;
    p->globalBallY = 0;
    p->xSpeed = 0;
    p->ySpeed = 0;
    p->direction = 0;
    p->jumpOffset = 0;
    p->ballState = BALL_STATE_NORMAL;
    p->sizeState = sizeState;
    
    // Установка размера в зависимости от состояния
    if (sizeState == SMALL_SIZE_STATE) {
        p->ballSize = NORMAL_SIZE;
        p->mHalfBallSize = HALF_NORMAL_SIZE;
    } else if (sizeState == LARGE_SIZE_STATE) {
        p->ballSize = ENLARGED_SIZE;
        p->mHalfBallSize = HALF_ENLARGED_SIZE;
    }
    
    p->mGroundedFlag = 0;
    p->mCDRubberFlag = 0;
    p->mCDRampFlag = 0;
    
    p->speedBonusCntr = 0;
    p->gravBonusCntr = 0;
    p->jumpBonusCntr = 0;
    p->popCntr = 0;
    p->slideCntr = 0;
    
    p->isInWater = false;
    
    // Подталкивание большого мяча при инициализации в тесном месте (точно как в Java createBufferFocused:156-173)
    if (p->sizeState == LARGE_SIZE_STATE && !collisionDetection(p, p->xPos, p->yPos)) {
        int offset = STUCK_BALL_OFFSET;
        
        // Точно 3 проверки как в оригинале:
        if (collisionDetection(p, p->xPos - offset, p->yPos)) {
            // 1. Влево
            p->xPos -= offset;
        } else if (collisionDetection(p, p->xPos, p->yPos - offset)) {
            // 2. Вверх
            p->yPos -= offset;
        } else if (collisionDetection(p, p->xPos - offset, p->yPos - offset)) {
            // 3. Влево-вверх
            p->xPos -= offset;
            p->yPos -= offset;
        }
        // Если ни одно направление не подошло - оставляем как есть (как в оригинале)
    }
}

// Увеличение мяча (портировано из enlargeBall())
void enlarge_ball(Player* p) {
    if (p->sizeState == LARGE_SIZE_STATE) return; // Уже большой
    
    p->sizeState = LARGE_SIZE_STATE;
    p->ballSize = ENLARGED_SIZE;
    p->mHalfBallSize = HALF_ENLARGED_SIZE;
    
    // Полная логика поиска свободного места как в оригинале
    int offset = 2;
    int found_free_space = 0;
    
    while (!found_free_space) {
        found_free_space = 1;
        
        // Проверяем все 6 направлений в порядке приоритета как в Ball.java:
        if (collisionDetection(p, p->xPos, p->yPos - offset)) {
            // 1. Вверх (приоритет)
            p->yPos -= offset;
        } else if (collisionDetection(p, p->xPos - offset, p->yPos - offset)) {
            // 2. Лево-вверх
            p->xPos -= offset;
            p->yPos -= offset;
        } else if (collisionDetection(p, p->xPos + offset, p->yPos - offset)) {
            // 3. Право-вверх
            p->xPos += offset;
            p->yPos -= offset;
        } else if (collisionDetection(p, p->xPos, p->yPos + offset)) {
            // 4. Вниз
            p->yPos += offset;
        } else if (collisionDetection(p, p->xPos - offset, p->yPos + offset)) {
            // 5. Лево-вниз
            p->xPos -= offset;
            p->yPos += offset;
        } else if (collisionDetection(p, p->xPos + offset, p->yPos + offset)) {
            // 6. Право-вниз
            p->xPos += offset;
            p->yPos += offset;
        } else {
            // Не нашли свободное место - увеличиваем радиус поиска
            found_free_space = 0;
            offset++; // Java: b++
        }
    }
}

// Уменьшение мяча (портировано из shrinkBall())
void shrink_ball(Player* p) {
    if (p->sizeState == SMALL_SIZE_STATE) return; // Уже маленький
    
    p->sizeState = SMALL_SIZE_STATE;
    p->ballSize = NORMAL_SIZE;
    p->mHalfBallSize = HALF_NORMAL_SIZE;
    
    // Проверка позиции после уменьшения как в оригинале
    int offset = 2;
    if (collisionDetection(p, p->xPos, p->yPos + offset)) {
        p->yPos += offset;
    } else if (collisionDetection(p, p->xPos, p->yPos - offset)) {
        p->yPos -= offset;
    }
    // Если оба направления заняты - остаемся на месте
}

// Лопание мяча (портировано из popBall())
void pop_ball(Player* p) {
    // Проверка читерского бессмертия (как в оригинале Java !mCanvas.mInvincible)
    if (g_game.invincible_cheat) return;
    
    p->ballState = BALL_STATE_POPPED;
    p->popCntr = POPPED_FRAMES;  // Анимация лопания (как в Java)
    p->xSpeed = 0;
    p->ySpeed = 0;
    p->jumpOffset = 0;  // Сброс смещения прыжка
    
    // Воспроизводим звук лопания мяча
    sound_play_pop();
    
    // Сброс бонусов (как в Java)
    p->speedBonusCntr = 0;
    p->gravBonusCntr = 0;
    p->jumpBonusCntr = 0;
    
    // Уменьшаем жизни (как в Java)
    g_game.numLives--;
}

// Установка направления (битовые флаги)
// MOVE_DOWN не используется - падение происходит автоматически через гравитацию
void set_direction(Player* p, MoveDirection dir) {
    if (dir == MOVE_LEFT || dir == MOVE_RIGHT || dir == MOVE_UP) {
        p->direction |= (MoveMask)dir;
    }
}

// Сброс направления
// MOVE_DOWN не используется - падение происходит автоматически через гравитацию
void release_direction(Player* p, MoveDirection dir) {
    if (dir == MOVE_LEFT || dir == MOVE_RIGHT || dir == MOVE_UP) {
        p->direction &= (MoveMask)~dir;
    }
}

// Пиксельная коллизия с квадратным тайлом (портировано из Ball.java)
static bool squareCollide(Player* p, int tileRow, int tileCol) {
    // Точная копия Java Ball.squareCollide()
    int i = tileCol * 12;  // Java: int i = paramInt2 * 12
    int j = tileRow * 12;  // Java: int j = paramInt1 * 12

    int k = p->globalBallX - i;  // Java: int k = this.globalBallX - i
    int m = p->globalBallY - j;  // Java: int m = this.globalBallY - j

    int x_start, x_end, y_start, y_end;

    // Границы пересечения мяча с тайлом по X/Y (канон Java)
    if (k >= 0) {
        x_start = k;
        x_end = 12;
    } else {
        x_start = 0;
        x_end = p->ballSize + k;
    }

    if (m >= 0) {
        y_start = m;
        y_end = 12;
    } else {
        y_start = 0;
        y_end = p->ballSize + m;
    }

    // Java: if (n > 12) n = 12; if (i1 > 12) i1 = 12;
    if (x_end > 12) x_end = 12;
    if (y_end > 12) y_end = 12;

    // Выбор маски мяча как в Java - разные типы для разных размеров
    if (p->ballSize == 16) {
        const uint8_t (*largeBallData)[16] = (const uint8_t (*)[16])LARGE_BALL_DATA;
        // Java: for (byte b3 = b1; b3 < n; b3++) for (byte b = b2; b < i1; b++)
        for (int b3 = x_start; b3 < x_end; b3++) {
            for (int b = y_start; b < y_end; b++) {
                // Java: if (arrayOfByte[b - m][b3 - k] != 0) return true;
                if (largeBallData[b - m][b3 - k] != 0) {
                    return true;
                }
            }
        }
    } else {
        const uint8_t (*smallBallData)[12] = (const uint8_t (*)[12])SMALL_BALL_DATA;
        // Java: for (byte b3 = b1; b3 < n; b3++) for (byte b = b2; b < i1; b++)
        for (int b3 = x_start; b3 < x_end; b3++) {
            for (int b = y_start; b < y_end; b++) {
                // Java: if (arrayOfByte[b - m][b3 - k] != 0) return true;
                if (smallBallData[b - m][b3 - k] != 0) {
                    return true;
                }
            }
        }
    }

    return false;
}

// Коллизия с треугольной рампой (100% точный канон Java)
static bool triangleCollide(Player* p, int tileRow, int tileCol, int tileID) {
    // Точный канон Java: координаты тайла в пикселях
    int i = tileCol * TILE_SIZE;  // paramInt2 * 12
    int j = tileRow * TILE_SIZE;  // paramInt1 * 12
    
    // Локальные координаты шара в тайле
    int k = p->globalBallX - i;
    int m = p->globalBallY - j;
    
    // Смещения ориентации (канон Java)
    int b1 = 0, b2 = 0;
    switch (tileID) {
        case 30: case 34:
            b2 = 11; b1 = 11;
            break;
        case 31: case 35:
            b2 = 11;
            break;
        case 33: case 37:
            b1 = 11;
            break;
        // case 32: case 36: без смещений
    }
    
    // Границы цикла (канон Java)
    int b3, n, b4, i1;

    // Вычисляем границы пересечения мяча с тайлом (используем общий helper)
    clip_to_tile_bounds(k, p->ballSize, &b3, &n);
    clip_to_tile_bounds(m, p->ballSize, &b4, &i1);
    
    // Обрезка границ (канон Java)
    if (n > TILE_SIZE) n = TILE_SIZE;
    if (i1 > TILE_SIZE) i1 = TILE_SIZE;
    
    // Выбор данных мяча с правильными типами
    if (p->ballSize == ENLARGED_SIZE) {
        const uint8_t (*largeBallData)[ENLARGED_SIZE] = (const uint8_t (*)[ENLARGED_SIZE])LARGE_BALL_DATA;
        // Двойной цикл точно как в Java
        for (int b5 = b3; b5 < n; b5++) {
            for (int b = b4; b < i1; b++) {
                // Точная формула из Java: Math.abs() и обращение к ballData[b-m][b5-k]
                int ballY = b - m;
                int ballX = b5 - k;
                if (ballY >= 0 && ballY < ENLARGED_SIZE && ballX >= 0 && ballX < ENLARGED_SIZE &&
                    (TRI_TILE_DATA[abs(b - b2)][abs(b5 - b1)] & largeBallData[ballY][ballX]) != 0) {
                    if (!p->mGroundedFlag) {
                        redirectBall(p, tileID);
                    }
                    return true;
                }
            }
        }
    } else {
        const uint8_t (*smallBallData)[NORMAL_SIZE] = (const uint8_t (*)[NORMAL_SIZE])SMALL_BALL_DATA;
        // Двойной цикл точно как в Java
        for (int b5 = b3; b5 < n; b5++) {
            for (int b = b4; b < i1; b++) {
                // Точная формула из Java: Math.abs() и обращение к ballData[b-m][b5-k]
                int ballY = b - m;
                int ballX = b5 - k;
                if (ballY >= 0 && ballY < NORMAL_SIZE && ballX >= 0 && ballX < NORMAL_SIZE &&
                    (TRI_TILE_DATA[abs(b - b2)][abs(b5 - b1)] & smallBallData[ballY][ballX]) != 0) {
                    if (!p->mGroundedFlag) {
                        redirectBall(p, tileID);
                    }
                    return true;
                }
            }
        }
    }
    
    return false;
}

// Перенаправление мяча при столкновении с рампой (портировано из Ball.java)
static void redirectBall(Player* p, int tileID) {
    int oldXSpeed = p->xSpeed;
    
    switch (tileID) {
        case 35:
        case 37:
            // Поворот на 90°: (x,y) -> (y,x)
            p->xSpeed = p->ySpeed;
            p->ySpeed = oldXSpeed;
            break;
            
        case 31:
        case 33:
            // Поворот на 90° с уменьшением пополам: (x,y) -> (y/2, x/2) (как в Java >> 1)
            p->xSpeed = (p->ySpeed >> 1);
            p->ySpeed = (oldXSpeed >> 1);
            break;
            
        case 34:
        case 36:
            // Отражение с поворотом: (x,y) -> (-y, -x)
            p->xSpeed = -p->ySpeed;
            p->ySpeed = -oldXSpeed;
            break;
            
        case 30:
        case 32:
            // Отражение с поворотом и уменьшением: (x,y) -> (-(y/2), -(x/2)) (как в Java >> 1)
            p->xSpeed = -(p->ySpeed >> 1);
            p->ySpeed = -(oldXSpeed >> 1);
            break;
            
        default:
            // На всякий случай - если неизвестный тайл, останавливаем
            p->xSpeed = 0;
            p->ySpeed = 0;
            break;
    }
}

// Полная функция проверки коллизий (портировано из Ball.java)
static bool collisionDetection(Player* p, int testX, int testY) {
    // Временно обновляем глобальные координаты для тестирования
    int b = 0;
    if (testY < 0) {
        b = 12;
    }
    
    // Определяем диапазон тайлов для проверки (как в Java i,j,k,m в порядке Java)
    int i = (testX - p->mHalfBallSize) / TILE_SIZE;
    int j = (testY - b - p->mHalfBallSize) / TILE_SIZE;
    
    // Устанавливаем globalBallX/Y для squareCollide/triangleCollide
    p->globalBallX = testX - p->mHalfBallSize;
    p->globalBallY = testY - p->mHalfBallSize;
    
    // Смещения прокрутки экрана (в C нет прокрутки, добавляем 0 для соответствия Java)
    // В Java: if (this.xPos < this.mCanvas.divisorLine) { this.globalBallX += this.mCanvas.tileX * 12; ... }
    // В C нет прокрутки экрана, поэтому добавляем 0 (как если бы tileX=tileY=0)
    
    // Определяем конец диапазона
    int k = (testX - 1 + p->mHalfBallSize) / TILE_SIZE + 1;
    int m = (testY - b - 1 + p->mHalfBallSize) / TILE_SIZE + 1;
    
    bool canMove = true;

    // Проверяем все пересекающиеся тайлы (как в Java n, i1)
    // Порядок обхода как в Java: X-снаружи, Y-внутри
    // НЕ прерываем при canMove == false, чтобы корректно выставлялись флаги
    for (int n = i; n < k; n++) {
        for (int i1 = j; i1 < m; i1++) {
            canMove = testTile(p, i1, n, canMove);
        }
    }
    
    return canMove;
}

// Проверка конкретного тайла (портировано из Ball.java testTile)
static bool testTile(Player* p, int tileY, int tileX, bool canMove) {
    if (tileY >= g_level.height || tileY < 0 || tileX >= g_level.width || tileX < 0) {
        return false;  // За пределами карты - коллизия
    }
    
    if (p->ballState == BALL_STATE_POPPED) {
        return true;  // Лопнувший мяч проходит сквозь все
    }
    
    int tile = g_level.tileMap[tileY][tileX];
    int tileID = tile & TILE_ID_MASK;  // Убираем флаги
    
    // Получаем метаданные тайла (нужны для orientation в thinCollide)
    if ((uint32_t)tileID >= tile_meta_count()) {
        return canMove; // Неизвестный тайл - пропускаем
    }
    
    const TileMeta* tileMeta = &tile_meta_db()[tileID];
    (void)tileMeta; // Помечаем как используемую (для thinCollide через вызовы)
    
    // Прямая обработка коллизий по ID тайла (убран избыточный collision_type)
    if (tileID == 1) {
        // Кирпич ID 1 - точная копия Java case 1
        if (squareCollide(p, tileY, tileX)) {
            canMove = false;
            // В оригинале Java: сразу break, без дополнительных действий
        } else {
            // Только если НЕТ коллизии - устанавливаем mCDRampFlag
            p->mCDRampFlag = true;
        }
    } else if (tileID == 2) {
        // Резиновый блок ID 2 - точная копия Java case 2
        if (squareCollide(p, tileY, tileX)) {
            p->mCDRubberFlag = true;
            canMove = false;
            // В оригинале Java: сразу break, без дополнительных действий
        } else {
            // В оригинале Java: mCDRampFlag = true ТОЛЬКО для case 2 при отсутствии коллизии
            p->mCDRampFlag = true;
        }
    } else if (tileID >= 3 && tileID <= 6) {
        // Шипы - используют thinCollide с ориентацией
        if (thinCollide(p, tileY, tileX, tileID)) {
            canMove = false;
            pop_ball(p);  // Шипы лопают мяч (как в Java case 3,4,5,6)
        }
    } else if (tileID == 10) {
        // Движущиеся шипы - коллизия с движущимся объектом
        int objIndex = level_find_moving_object_at(tileX, tileY);
        if (objIndex != -1) {
            MovingObject* obj = level_get_moving_object(objIndex);
            if (obj) {
                // Вычисляем реальные координаты шипов как в Java
                int spikeX = obj->topLeft[0] * TILE_SIZE + obj->offset[0];
                int spikeY = obj->topLeft[1] * TILE_SIZE + obj->offset[1];
                
                // Проверяем пересечение мяча с областью шипов
                if (rectCollide(p->globalBallX, p->globalBallY, 
                               p->globalBallX + p->ballSize, p->globalBallY + p->ballSize,
                               spikeX, spikeY, spikeX + MOVING_SPIKE_PX, spikeY + MOVING_SPIKE_PX)) {
                    canMove = false;
                    pop_ball(p);  // Движущиеся шипы лопают мяч (как в Java case 10)
                }
            }
        }
    } else if (tileID >= 13 && tileID <= 24) {
        // Кольца (13-24) - используют thinCollide с специальной логикой
        if (thinCollide(p, tileY, tileX, tileID)) {
            // Большой мяч не может пройти через маленькие кольца (как в Java ballSize == 16)
            if ((tileID == 13 || tileID == 14 || tileID == 15 || tileID == 16 || tileID == 17 || tileID == 18 || tileID == 19 || tileID == 20) && p->sizeState == LARGE_SIZE_STATE) {
                canMove = false;
            } else {
                // ВАЖНО: Нижняя половина вертикального кольца пропускает без твёрдой кромки (канон Java)
                // Cases 14, 18, 22, 26 - все нижние части вертикальных колец НЕ используют edgeCollide
                // Это создает реалистичную физику баскетбольного кольца: застрять можно на верхнем ободе,
                // но снизу мяч проваливается свободно
                if (tileID == 14 || tileID == 18 || tileID == 22 || tileID == 26) {
                    // Свободный проход через нижнюю часть кольца
                    if (tileID == 14 || tileID == 22) {
                        // Активные кольца - засчитываем проход
                        game_ring_collected(tileX, tileY, tileID);
                    }
                    // Case 18, 26 (неактивные) - только проход без сбора
                } else {
                    // Остальные кольца проверяют edgeCollide для блокировки при касании края
                    if (edgeCollide(p, tileY, tileX, tileID)) {
                        canMove = false; // Блокируем движение при попадании в край (как в Java)
                    } else {
                        // Успешно прошли через центр кольца
                        if ((tileID >= 13 && tileID <= 16) || (tileID >= 21 && tileID <= 24)) {
                            // Активные кольца (маленькие 13-16 и большие 21-24) - засчитываем сбор
                            game_ring_collected(tileX, tileY, tileID);
                        }
                        // Case 17-20, 25-28 (неактивные) - только проход без сбора
                    }
                }
                // canMove может быть false если попали в край кольца
            }
        }
    } else if (tileID == 25 || tileID == 27 || tileID == 28) {
        // Большие неактивные кольца (Java case 25,27,28) - ТОЛЬКО edgeCollide
        if (edgeCollide(p, tileY, tileX, tileID)) {
            canMove = false; // Java: paramBoolean = false
        }
    } else if (tileID >= 30 && tileID <= 37) {
        // Рампы - используют triangleCollide
        if (triangleCollide(p, tileY, tileX, tileID)) {
            canMove = false;
            p->mCDRampFlag = true;
            
            // Резиновые рампы ID 34-37 устанавливают mCDRubberFlag (как в Java case 34,35,36,37)
            if (tileID >= 34 && tileID <= 37) {
                p->mCDRubberFlag = true;
            }
        }
    }
    // Остальные тайлы (ID 0, 7-9, 17-20, 25-29, etc.) - проходимые, без коллизий
    
    // Специальная логика для specific тайлов (не зависит от коллизии)
    switch (tileID) {
            
        // Тайл бонуса скорости
        case TILE_SPEED_BONUS:
            canMove = false; // Java: paramBoolean = false
            p->speedBonusCntr = BONUS_DURATION; // Java: this.speedBonusCntr = 300
            sound_play_pickup(); // Java: sound = this.mCanvas.mSoundPickup
            break;
            
        // Тайлы уменьшения мяча (deflator) - блокируют движение
        case TILE_DEFLATOR_FLOOR: case TILE_DEFLATOR_LEFT_WALL: case TILE_DEFLATOR_CEILING: case TILE_DEFLATOR_RIGHT_WALL:
            canMove = false; // Java: paramBoolean = false
            if (p->ballSize == ENLARGED_SIZE) { // Java: только большой мяч
                shrink_ball(p);
            }
            break;
            
        // Тайлы увеличения мяча (inflator) - используют thinCollide как в Java
        case TILE_INFLATOR_FLOOR: case TILE_INFLATOR_LEFT_WALL: case TILE_INFLATOR_CEILING: case TILE_INFLATOR_RIGHT_WALL:
            if (thinCollide(p, tileY, tileX, tileID)) {
                canMove = false; // Java: paramBoolean = false
                if (p->ballSize == NORMAL_SIZE) { // Java: только маленький мяч (ballSize == 12)
                    enlarge_ball(p);
                }
            }
            break;
        
        // Чекпоинт (Java case 7: строки 633-639)
        case TILE_CHECKPOINT:
            game_add_score(200);             // add2Score(200) - как в оригинале!
            game_set_respawn(tileX, tileY);  // Событие: чекпоинт активирован
            break;
            
        // Выход (Java case 9: проверяет открыта ли дверь)
        case TILE_EXIT:
            if (game_exit_is_open()) {
                // Дверь открыта - завершить уровень (как mExitFlag = true в Java)
                game_complete_level();
            } else {
                // Дверь закрыта - заблокировать движение
                return false;
            }
            break;
            
        // Дополнительная жизнь (Java case 29: Ball.java:800-810)
        case TILE_EXTRA_LIFE:
            game_add_extra_life();  // Событие: дополнительная жизнь собрана
            level_set_id(tileX, tileY, 0);  // Убираем тайл (Java: = 128, у нас 0 = пустота)
            break;
        
        
        // Бонусы гравитации (Java case 47-50: gravBonusCntr = 300)
        case TILE_GRAVITY_FLOOR: case TILE_GRAVITY_LEFT_WALL: case TILE_GRAVITY_CEILING: case TILE_GRAVITY_RIGHT_WALL:
            canMove = false; // Java: paramBoolean = false
            p->gravBonusCntr = BONUS_DURATION; // Java: this.gravBonusCntr = 300
            sound_play_pickup();
            break;
            
        // Бонусы прыжков (Java case 51-54: jumpBonusCntr = 300)
        case TILE_JUMP_FLOOR: case TILE_JUMP_LEFT_WALL: case TILE_JUMP_CEILING: case TILE_JUMP_RIGHT_WALL:
            canMove = false; // Java: paramBoolean = false
            p->jumpBonusCntr = BONUS_DURATION; // Java: this.jumpBonusCntr = 300
            sound_play_pickup();
            break;
        
        default:
            // Остальные неизвестные тайлы пропускаем (как пустые)
            break;
    }
    
    return canMove;  // Возвращаем текущее состояние
}


// Полностью портированная физика из Ball.java (100% Java-совместимая)
void player_update(Player* p) {
    // Обработка анимации лопания (как в Java Ball.java:948-955)
    if (p->ballState == BALL_STATE_POPPED) {
        p->jumpOffset = 0;  // Сброс смещения при анимации
        p->popCntr--;       // Уменьшаем счетчик анимации
        if (p->popCntr == 0) {
            p->ballState = BALL_STATE_DEAD;  // Переход в состояние смерти
            // Проверка game over делается в game.c при обработке DEAD
        }
        return; // Блокируем всю остальную физику во время анимации
    }
    
    // Устанавливаем globalBallX/Y для текущей позиции перед первым update
    // (для маленького мяча не вызывается в player_init, но нужен для первого collisionDetection)
    if (p->globalBallX == 0 && p->globalBallY == 0) {
        p->globalBallX = p->xPos - p->mHalfBallSize;
        p->globalBallY = p->yPos - p->mHalfBallSize;
    }
    
    // Определение параметров гравитации (точно как в Java 915-937)
    int gravity, gravityStep;
    bool reverseGrav = false;
    
    // Проверка флага воды по центру мяча (Java 898-899: m = xPos/12, n = yPos/12)
    int tileX, tileY;
    player_center_tile(p, &tileX, &tileY);
    
    if (tileX >= 0 && tileX < g_level.width && tileY >= 0 && tileY < g_level.height) {
        int tile = g_level.tileMap[tileY][tileX];
        p->isInWater = (tile & TILE_FLAG_WATER) ? true : false;
    } else {
        p->isInWater = false;
    }
    
    // Установка гравитации в зависимости от воды и размера (Java 916-937)
    if (p->isInWater) {
        if (p->ballSize == ENLARGED_SIZE) {
            gravity = UWATER_LARGE_MAX_GRAVITY;  // k = -30 (всплывает)
            gravityStep = LARGE_UWATER_GRAVITY_ACCELL; // j = -2
            if (p->mGroundedFlag) {
                p->ySpeed = -BASE_GRAVITY; // Java 921: this.ySpeed = -10;
            }
        } else {
            gravity = UWATER_MAX_GRAVITY;   // k = 42
            gravityStep = UWATER_GRAVITY_ACCELL; // j = 6
        }
    } else {
        if (p->ballSize == ENLARGED_SIZE) {
            gravity = LARGE_MAX_GRAVITY;   // k = 38
            gravityStep = LARGE_GRAVITY_ACCELL; // j = 3
        } else {
            gravity = NORMAL_MAX_GRAVITY;   // k = 80
            gravityStep = NORMAL_GRAVITY_ACCELL; // j = 4
        }
    }
    
    // Бонус обратной гравитации (Java 940-951)
    if (p->gravBonusCntr > 0) {
        reverseGrav = true;
        gravity *= -1;
        gravityStep *= -1;
        p->gravBonusCntr--;
        if (p->gravBonusCntr == 0) {
            reverseGrav = false;
            p->mGroundedFlag = false;
            gravity *= -1;
            gravityStep *= -1;
        }
    }
    
    // Бонус прыжка (Java 953-962)
    if (p->jumpBonusCntr > 0) {
        if (-1 * abs(p->jumpOffset) > JUMP_BONUS_STRENGTH) {
            if (reverseGrav) {
                p->jumpOffset = -JUMP_BONUS_STRENGTH;
            } else {
                p->jumpOffset = JUMP_BONUS_STRENGTH;
            }
        }
        p->jumpBonusCntr--;
    }
    
    // Счётчик скольжения (Java 964-967)
    p->slideCntr++;
    if (p->slideCntr == 3) {
        p->slideCntr = 0;
    }
    
    // Ограничение максимальной скорости (Java 969-978)
    clamp_speed(p);

    // Удалён неканоничный кламп X-скорости при вертикальном вводе
    
    // === ФИЗИКА ПО ОСИ Y === (Java 980-1069)
    for (int i = 0; i < abs(p->ySpeed) / MOVEMENT_STEP_DIVISOR; i++) {
        int yStep = 0;
        if (p->ySpeed != 0) {
            yStep = (p->ySpeed < 0) ? -1 : 1;
        }
        
        // Попытка движения (Java 989)
        bool canMoveY = collisionDetection(p, p->xPos, p->yPos + yStep);
        if (canMoveY) {
            p->yPos += yStep;
            p->mGroundedFlag = false;
            
            // Специальная логика для подводного большого мяча (Java 995-1006)
            if (gravity == -30) { // Подводный большой мяч
                // Java 996: n = this.mCanvas.tileY + this.yPos / 12
                // Пересчитывается только Y, X остаётся фиксированным (m от начала кадра)
                int unused_tileX, currentTileY;
                player_center_tile(p, &unused_tileX, &currentTileY);
                
                if (currentTileY >= 0 && currentTileY < g_level.height && 
                    tileX >= 0 && tileX < g_level.width) {  // tileX от центра мяча (как m в Java)
                    int currentTile = g_level.tileMap[currentTileY][tileX];
                    if ((currentTile & TILE_FLAG_WATER) == 0) {
                        // Вышел из воды - замедляемся
                        p->ySpeed >>= 1;
                        if (p->ySpeed <= MIN_BOUNCE_SPEED && p->ySpeed >= -MIN_BOUNCE_SPEED) {
                            p->ySpeed = 0;
                        }
                    }
                }
            }
        } else {
            // Коллизия - пытаемся скользить по рампе (Java 1011-1025)
            // Канон Java: условие только по mCDRampFlag, xSpeed < 10 и slideCntr == 0
            if (p->mCDRampFlag && p->xSpeed < 10 && p->slideCntr == 0) {
                int slideStep = 1;
                if (collisionDetection(p, p->xPos + slideStep, p->yPos + yStep)) {
                    p->xPos += slideStep;
                    p->yPos += yStep;
                    p->mCDRampFlag = false;
                } else if (collisionDetection(p, p->xPos - slideStep, p->yPos + yStep)) {
                    p->xPos -= slideStep;
                    p->yPos += yStep;
                    p->mCDRampFlag = false;
                }
            }
            
            // Отскок от препятствия (Java 1027-1055)
            if (yStep > 0 || (reverseGrav && yStep < 0)) {
                // Отскок от пола/препятствия (Java: this.ySpeed = this.ySpeed * -1 / 2)
                // ВАЖНО: умножение на -1 ДО деления даёт правильное округление к нулю
                p->ySpeed = p->ySpeed * -1 / 2;
                p->mGroundedFlag = true;
                
                // Резиновый отскок (Java 1032-1042)
                if (p->mCDRubberFlag && (p->direction & MOVE_UP)) {
                    p->mCDRubberFlag = false;
                    if (reverseGrav) {
                        p->jumpOffset += MIN_BOUNCE_SPEED;
                    } else {
                        p->jumpOffset += -MIN_BOUNCE_SPEED;
                    }
                } else if (p->jumpBonusCntr == 0) {
                    p->jumpOffset = 0;
                }
                
                // Нормализация скорости отскока (Java 1046-1051)
                if (p->ySpeed < MIN_BOUNCE_SPEED && p->ySpeed > -MIN_BOUNCE_SPEED) {
                    if (reverseGrav) {
                        p->ySpeed = -MIN_BOUNCE_SPEED;
                    } else {
                        p->ySpeed = MIN_BOUNCE_SPEED;
                    }
                }
                break;
            }
            
            // Удар о потолок (Java 1057-1067)
            if (yStep < 0 || (reverseGrav && yStep > 0)) {
                if (reverseGrav) {
                    p->ySpeed = -ROOF_COLLISION_SPEED;
                } else {
                    p->ySpeed = ROOF_COLLISION_SPEED;
                }
            }
        }
    }
    
    // Применение гравитации (Java 1071-1082) - выполняется всегда после Y-фазы
    if (reverseGrav) {
        if (gravityStep == -2 && p->ySpeed < gravity) { // Подводный большой мяч
            p->ySpeed += gravityStep;
            if (p->ySpeed > gravity) p->ySpeed = gravity;
        } else if (!p->mGroundedFlag && p->ySpeed > gravity) {
            p->ySpeed += gravityStep;
            if (p->ySpeed < gravity) p->ySpeed = gravity;
        }
    } else {
        if (gravityStep == -2 && p->ySpeed > gravity) { // Подводный большой мяч
            p->ySpeed += gravityStep;
            if (p->ySpeed < gravity) p->ySpeed = gravity;
        } else if (!p->mGroundedFlag && p->ySpeed < gravity) {
            p->ySpeed += gravityStep;
            if (p->ySpeed > gravity) p->ySpeed = gravity;
        }
    }

    // Накопление jumpOffset для большого мяча (Java 1118-1124)
    if (p->ballSize == ENLARGED_SIZE && p->jumpBonusCntr == 0) {
        if (reverseGrav) {
            p->jumpOffset += 5;
        } else {
            p->jumpOffset += -5;
        }
    }
    
    // === УПРАВЛЕНИЕ ГОРИЗОНТАЛЬНЫМ ДВИЖЕНИЕМ === (Java аналог)
    int maxSpeed = (p->speedBonusCntr > 0) ? MAX_HORZ_BONUS_SPEED : MAX_HORZ_SPEED;
    if (p->speedBonusCntr > 0) p->speedBonusCntr--;
    
    if ((p->direction & MOVE_RIGHT) && p->xSpeed < maxSpeed) {
        p->xSpeed += HORZ_ACCELL;
    } else if ((p->direction & MOVE_LEFT) && p->xSpeed > -maxSpeed) {
        p->xSpeed -= HORZ_ACCELL;
    } else if (p->xSpeed > 0) {
        p->xSpeed -= FRICTION_DECELL;
    } else if (p->xSpeed < 0) {
        p->xSpeed += FRICTION_DECELL;
    }
    
    // === ПРЫЖОК ===
    if (p->mGroundedFlag && (p->direction & MOVE_UP)) {
        if (reverseGrav) {
            p->ySpeed = -JUMP_STRENGTH + p->jumpOffset;
        } else {
            p->ySpeed = JUMP_STRENGTH + p->jumpOffset;
        }
        p->mGroundedFlag = false;
    }
    
    // === ФИЗИКА ПО ОСИ X === (Java 1135-1166)
    for (int i = 0; i < abs(p->xSpeed) / MOVEMENT_STEP_DIVISOR; i++) {
        int xStep = 0;
        if (p->xSpeed != 0) {
            xStep = (p->xSpeed < 0) ? -1 : 1;
        }
        
        // Java 1142: обычное движение по X
        if (collisionDetection(p, p->xPos + xStep, p->yPos)) {
            p->xPos += xStep;
        } else if (p->mCDRampFlag) {
            // Java 1144-1164: диагональное скольжение по рампе
            p->mCDRampFlag = false; // Java 1145: сброс, но может быть восстановлен в collisionDetection()
            int diagonalStep = reverseGrav ? 1 : -1; // Java: bool ? 1 : -1

            // Пробуем диагональ 1: (xStep, diagonalStep)
            if (collisionDetection(p, p->xPos + xStep, p->yPos + diagonalStep)) {
                p->xPos += xStep;
                p->yPos += diagonalStep;
            }
            // Пробуем диагональ 2: (xStep, -diagonalStep)
            else if (collisionDetection(p, p->xPos + xStep, p->yPos - diagonalStep)) {
                p->xPos += xStep;
                p->yPos -= diagonalStep;
            }
            // Если диагонали не сработали - отскок
            else {
                // Java: this.xSpeed = -(this.xSpeed >> 1)
                p->xSpeed = -(p->xSpeed >> 1);
            }
        } else {
            // Обычный отскок без рампы
            if (p->xSpeed != 0) {
                // Java: this.xSpeed = -(this.xSpeed >> 1)
                p->xSpeed = -(p->xSpeed >> 1);
            }
        }
    }

    // Границы уровня
    int levelW = g_level.width * TILE_SIZE;
    if (p->xPos < p->mHalfBallSize) p->xPos = p->mHalfBallSize;
    if (p->xPos > levelW - p->mHalfBallSize) p->xPos = levelW - p->mHalfBallSize;
}

static bool rectCollide(int x1, int y1, int x2, int y2, int rx1, int ry1, int rx2, int ry2) {
    return (x1 <= rx2 && y1 <= ry2 && rx1 <= x2 && ry1 <= y2);
}

// Тонкие коллизии универсальные на основе ориентации тайла (улучшенная версия Java thinCollide)
static bool thinCollide(Player* p, int tileRow, int tileCol, int tileID) {
    int tilePixelX = tileCol * TILE_SIZE;  // i в Java
    int tilePixelY = tileRow * TILE_SIZE;  // j в Java
    int tileRight = tilePixelX + TILE_SIZE;  // k в Java
    int tileBottom = tilePixelY + TILE_SIZE; // m в Java

    // Прямая портировка Java switch (Ball.java:520-538)
    switch (tileID) {
        // Горизонтальное сужение (i += 4, k -= 4)
        case 3:   // SPIKE_UP
        case 5:   // SPIKE_DOWN
        case 9:   // EXIT_TILE
        case 13:  // HOOP_ACTIVE_VERT_TOP
        case 14:  // HOOP_ACTIVE_VERT_BOTTOM
        case 17:  // HOOP_INACTIVE_VERT_TOP
        case 18:  // HOOP_INACTIVE_VERT_BOTTOM
        case 21:  // LARGE_HOOP_ACTIVE_VERT_TOP
        case 22:  // LARGE_HOOP_ACTIVE_VERT_BOTTOM
        case 43:  // INFLATOR_FLOOR
        case 45:  // INFLATOR_CEILING
            tilePixelX += THIN_TILE_SIZE;  // i += 4
            tileRight -= THIN_TILE_SIZE;   // k -= 4
            break;

        // Вертикальное сужение (j += 4, m -= 4)
        case 4:   // SPIKE_LEFT
        case 6:   // SPIKE_RIGHT
        case 15:  // HOOP_ACTIVE_HORIZ_LEFT
        case 16:  // HOOP_ACTIVE_HORIZ_RIGHT
        case 19:  // HOOP_INACTIVE_HORIZ_LEFT
        case 20:  // HOOP_INACTIVE_HORIZ_RIGHT
        case 23:  // LARGE_HOOP_ACTIVE_HORIZ_LEFT
        case 24:  // LARGE_HOOP_ACTIVE_HORIZ_RIGHT
        case 44:  // INFLATOR_LEFT_WALL
        case 46:  // INFLATOR_RIGHT_WALL
            tilePixelY += THIN_TILE_SIZE;  // j += 4
            tileBottom -= THIN_TILE_SIZE;  // m -= 4
            break;

        // Тайлы без сужения (остальные используют полные границы)
        default:
            // Без модификации границ
            break;
    }

    // Проверка пересечения мяча с модифицированным тайлом (точно как в Java)
    return rectCollide(p->globalBallX, p->globalBallY,
                      p->globalBallX + p->ballSize, p->globalBallY + p->ballSize,
                      tilePixelX, tilePixelY, tileRight, tileBottom);
}

// Проверка коллизий с краями для колец (портировано из Ball.java edgeCollide) 
static bool edgeCollide(Player* p, int tileRow, int tileCol, int tileID) {
    int tilePixelX = tileCol * TILE_SIZE;
    int tilePixelY = tileRow * TILE_SIZE;
    int tileRight = tilePixelX + TILE_SIZE;
    int tileBottom = tilePixelY + TILE_SIZE;
    
    // Специальная логика для разных типов колец (Java switch 505-580)
    switch (tileID) {
        // Маленькие вертикальные кольца (Java case 13, 17)
        case 13: case 17:
            tilePixelX += 6;
            tileRight -= 6;  
            tileBottom -= 11; // m -= 11
            return rectCollide(p->globalBallX, p->globalBallY,
                              p->globalBallX + p->ballSize, p->globalBallY + p->ballSize,
                              tilePixelX, tilePixelY, tileRight, tileBottom);
            
        // Маленькие вертикальные кольца - нижняя часть (Java case 14, 18)
        case 14: case 18:
            tilePixelX += 6;
            tileRight -= 6;
            tilePixelY += 11; // j += 11 как в оригинальном Java
            return rectCollide(p->globalBallX, p->globalBallY,
                              p->globalBallX + p->ballSize, p->globalBallY + p->ballSize,
                              tilePixelX, tilePixelY, tileRight, tileBottom);
                              
        // Большие вертикальные кольца - нижняя часть (Java case 22, 26)
        case 22: case 26:
            tilePixelX += 6;
            tileRight -= 6;
            tilePixelY += 11; // j += 11 как в оригинальном Java
            return rectCollide(p->globalBallX, p->globalBallY,
                              p->globalBallX + p->ballSize, p->globalBallY + p->ballSize,
                              tilePixelX, tilePixelY, tileRight, tileBottom);
            
        // Большие вертикальные кольца (Java case 21, 25)  
        case 21: case 25:
            tileBottom = tilePixelY; // m = j; j--
            tilePixelY--;
            tilePixelX += 6;
            tileRight -= 6;
            return rectCollide(p->globalBallX, p->globalBallY,
                              p->globalBallX + p->ballSize, p->globalBallY + p->ballSize,  
                              tilePixelX, tilePixelY, tileRight, tileBottom);
            
        // Маленькие горизонтальные кольца - левая часть (Java case 15, 19)
        case 15: case 19:
            tilePixelY += 6;  // j += 6
            tileBottom -= 6;  // m -= 6  
            tileRight -= 11;  // k -= 11
            return rectCollide(p->globalBallX, p->globalBallY,
                              p->globalBallX + p->ballSize, p->globalBallY + p->ballSize,
                              tilePixelX, tilePixelY, tileRight, tileBottom);
            
        // Маленькие горизонтальные кольца - правая часть (Java case 16, 20)
        case 16: case 20:
            tilePixelY += 6;  // j += 6
            tileBottom -= 6;  // m -= 6
            tilePixelX += 11; // i += 11
            return rectCollide(p->globalBallX, p->globalBallY,
                              p->globalBallX + p->ballSize, p->globalBallY + p->ballSize,
                              tilePixelX, tilePixelY, tileRight, tileBottom);
                              
        // Большие горизонтальные кольца - левая часть (Java case 23, 27)
        case 23: case 27:
            tilePixelY += 6;  // j += 6
            tileBottom -= 6;  // m -= 6  
            tileRight -= 11;  // k -= 11
            return rectCollide(p->globalBallX, p->globalBallY,
                              p->globalBallX + p->ballSize, p->globalBallY + p->ballSize,
                              tilePixelX, tilePixelY, tileRight, tileBottom);
                              
        // Большие горизонтальные кольца - правая часть (Java case 24, 28)
        case 24: case 28:
            tilePixelY += 6;  // j += 6
            tileBottom -= 6;  // m -= 6
            tilePixelX += 11; // i += 11
            return rectCollide(p->globalBallX, p->globalBallY,
                              p->globalBallX + p->ballSize, p->globalBallY + p->ballSize,
                              tilePixelX, tilePixelY, tileRight, tileBottom);
            
        // Остальные кольца - аналогичная логика
        default:
            return false;
    }
}
