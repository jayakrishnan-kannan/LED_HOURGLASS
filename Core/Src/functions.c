/* functions.c - FIXED VERSION */
#include "main.h"
#include "max7219.h"
#include <stdlib.h>

uint8_t delayHours = 0;
uint8_t delayMinutes = 1;
int gravity = 0;
bool alarmWentOff = false;
NonBlockDelay_t drop_delay;

struct coord {
    uint8_t x;
    uint8_t y;
};

// ==================== COORDINATE HELPERS ====================
struct coord getDown(uint8_t x, uint8_t y)  { struct coord c = {x-1, y+1}; return c; }
struct coord getLeft(uint8_t x, uint8_t y)  { struct coord c = {x-1, y};   return c; }
struct coord getRight(uint8_t x, uint8_t y) { struct coord c = {x, y+1};   return c; }

// ==================== MOVEMENT CHECKS ====================
uint8_t canGoLeft(uint8_t addr, uint8_t x, uint8_t y) {
    if (x == 0) return 0;
    return !MAX7219_GetXY(&lc, addr, getLeft(x,y).x, getLeft(x,y).y);
}

uint8_t canGoRight(uint8_t addr, uint8_t x, uint8_t y) {
    if (y == 7) return 0;
    return !MAX7219_GetXY(&lc, addr, getRight(x,y).x, getRight(x,y).y);
}

uint8_t canGoDown(uint8_t addr, uint8_t x, uint8_t y) {
    if (y == 7 || x == 0) return 0;
    if (!canGoLeft(addr, x, y)) return 0;
    if (!canGoRight(addr, x, y)) return 0;
    return !MAX7219_GetXY(&lc, addr, getDown(x,y).x, getDown(x,y).y);
}

// ==================== MOVE FUNCTIONS ====================
void goDown(uint8_t addr, uint8_t x, uint8_t y) {
    MAX7219_SetXY(&lc, addr, x, y, 0);
    MAX7219_SetXY(&lc, addr, getDown(x,y).x, getDown(x,y).y, 1);
}

void goLeft(uint8_t addr, uint8_t x, uint8_t y) {
    MAX7219_SetXY(&lc, addr, x, y, 0);
    MAX7219_SetXY(&lc, addr, getLeft(x,y).x, getLeft(x,y).y, 1);
}

void goRight(uint8_t addr, uint8_t x, uint8_t y) {
    MAX7219_SetXY(&lc, addr, x, y, 0);
    MAX7219_SetXY(&lc, addr, getRight(x,y).x, getRight(x,y).y, 1);
}

// ==================== CORE PARTICLE FUNCTIONS ====================
uint8_t moveParticle(uint8_t addr, uint8_t x, uint8_t y) {
    if (!MAX7219_GetXY(&lc, addr, x, y)) return 0;

    uint8_t canLeft  = canGoLeft(addr, x, y);
    uint8_t canRight = canGoRight(addr, x, y);

    if (!canLeft && !canRight) return 0;

    uint8_t canDown = canGoDown(addr, x, y);

    if (canDown) {
        goDown(addr, x, y);
    } else if (canLeft && !canRight) {
        goLeft(addr, x, y);
    } else if (canRight && !canLeft) {
        goRight(addr, x, y);
    } else if (rand() % 2) {
        goLeft(addr, x, y);
    } else {
        goRight(addr, x, y);
    }
    return 1;
}

void fill(uint8_t addr, uint8_t maxcount) {
    int count = 0;
    for (uint8_t slice = 0; slice < 15; ++slice) {
        uint8_t z = (slice < 8) ? 0 : slice - 7;
        for (uint8_t j = z; j <= slice - z; ++j) {
            uint8_t y = 7 - j;
            uint8_t x = slice - j;
            MAX7219_SetXY(&lc, addr, x, y, (++count <= maxcount));
        }
    }
}

uint8_t countParticles(uint8_t addr) {
    uint8_t c = 0;
    for (uint8_t y = 0; y < 8; y++)
        for (uint8_t x = 0; x < 8; x++)
            if (MAX7219_GetXY(&lc, addr, x, y)) c++;
    return c;
}

uint8_t updateMatrix(void) {
    uint8_t somethingMoved = 0;
    for (uint8_t slice = 0; slice < 15; ++slice) {
        uint8_t direction = rand() % 2;
        uint8_t z = (slice < 8) ? 0 : slice - 7;
        for (uint8_t j = z; j <= slice - z; ++j) {
            uint8_t y = direction ? (7 - j) : (7 - (slice - j));
            uint8_t x = direction ? (slice - j) : j;

            if (moveParticle(MATRIX_B, x, y)) somethingMoved = 1;
            if (moveParticle(MATRIX_A, x, y)) somethingMoved = 1;
        }
    }
    return somethingMoved;
}
uint32_t millis(void) {
	return HAL_GetTick();
}

long getDelayDrop(void) {
    return (long)delayMinutes + delayHours * 60;
}

uint8_t getTopMatrix(void) {
    return (gravity == 90) ? MATRIX_A : MATRIX_B;
//    return MATRIX_A;

}

void resetTime(void) {
    MAX7219_ClearDisplay(&lc, 0);
    MAX7219_ClearDisplay(&lc, 1);
    fill(getTopMatrix(), 60);                    // ← This was missing!
    drop_delay.start = HAL_GetTick();
    drop_delay.interval = getDelayDrop() * 1000UL;
}

uint8_t dropParticle(void) {
    if (HAL_GetTick() - drop_delay.start >= drop_delay.interval) {
        drop_delay.start = HAL_GetTick();
        drop_delay.interval = getDelayDrop() * 1000UL;

        if (gravity == 0 || gravity == 180) {
            uint8_t top_has = MAX7219_GetRawXY(&lc, MATRIX_A, 0, 0);
            uint8_t bot_has = MAX7219_GetRawXY(&lc, MATRIX_B, 7, 7);

            if ((top_has && !bot_has) || (!top_has && bot_has)) {
                MAX7219_InvertRawXY(&lc, MATRIX_A, 0, 0);   // Better to use Raw here
                MAX7219_InvertRawXY(&lc, MATRIX_B, 7, 7);

                HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
                HAL_Delay(10);
                HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
                return 1;
            }
        }
    }
    return 0;
}

void alarm(void) {
    for (int i = 0; i < 5; i++) {
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
        HAL_Delay(200);
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
        HAL_Delay(800);
    }
}
