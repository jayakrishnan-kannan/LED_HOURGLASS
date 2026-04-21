/* functions.c - FIXED VERSION v2 */
#include "main.h"
#include "max7219.h"
#include <stdlib.h>

uint8_t delayHours   = 0;
uint8_t delayMinutes = 1;
int     gravity      = 0;
bool    alarmWentOff = false;
NonBlockDelay_t drop_delay;

struct coord {
    uint8_t x;
    uint8_t y;
};

// ==================== GRAVITY-AWARE DIRECTION HELPERS ====================
// "Down"  = direction gravity pulls sand
// "Left"  = one of the two lateral neighbours sand can slide to
// "Right" = the other lateral neighbour

static struct coord getDownDirection(uint8_t x, uint8_t y) {
    struct coord c;
    switch (gravity) {
        case 90:  c.x = x - 1; c.y = y;     break;  // tilt right  → sand goes -x
        case 180: c.x = x;     c.y = y - 1; break;  // upside-down → sand goes -y
        case 270: c.x = x + 1; c.y = y;     break;  // tilt left   → sand goes +x
        default:  c.x = x;     c.y = y + 1; break;  // normal      → sand goes +y
    }
    return c;
}

static struct coord getLeftDirection(uint8_t x, uint8_t y) {
    struct coord c;
    switch (gravity) {
        case 90:  c.x = x;     c.y = y - 1; break;
        case 180: c.x = x - 1; c.y = y;     break;
        case 270: c.x = x;     c.y = y + 1; break;
        default:  c.x = x - 1; c.y = y;     break;
    }
    return c;
}

static struct coord getRightDirection(uint8_t x, uint8_t y) {
    struct coord c;
    switch (gravity) {
        case 90:  c.x = x;     c.y = y + 1; break;
        case 180: c.x = x + 1; c.y = y;     break;
        case 270: c.x = x;     c.y = y - 1; break;
        default:  c.x = x + 1; c.y = y;     break;
    }
    return c;
}

// ==================== BOUNDARY CHECK ====================
// uint8_t wraps around on underflow (e.g. 0-1 = 255) - catch that
static uint8_t coordValid(struct coord c) {
    return (c.x < 8 && c.y < 8);
}

// ==================== GRAVITY-AWARE MOVEMENT CHECKS ====================
static uint8_t canGoDown(uint8_t addr, uint8_t x, uint8_t y) {
    struct coord next = getDownDirection(x, y);
    if (!coordValid(next)) return 0;
    return !MAX7219_GetXY(&lc, addr, next.x, next.y);
}

static uint8_t canGoLeft(uint8_t addr, uint8_t x, uint8_t y) {
    struct coord next = getLeftDirection(x, y);
    if (!coordValid(next)) return 0;
    return !MAX7219_GetXY(&lc, addr, next.x, next.y);
}

static uint8_t canGoRight(uint8_t addr, uint8_t x, uint8_t y) {
    struct coord next = getRightDirection(x, y);
    if (!coordValid(next)) return 0;
    return !MAX7219_GetXY(&lc, addr, next.x, next.y);
}

// Down-Left diagonal: down then left  (for diagonal sliding)
static uint8_t canGoDownLeft(uint8_t addr, uint8_t x, uint8_t y) {
    struct coord d = getDownDirection(x, y);
    if (!coordValid(d)) return 0;
    struct coord dl = getLeftDirection(d.x, d.y);
    if (!coordValid(dl)) return 0;
    return !MAX7219_GetXY(&lc, addr, dl.x, dl.y);
}

static uint8_t canGoDownRight(uint8_t addr, uint8_t x, uint8_t y) {
    struct coord d = getDownDirection(x, y);
    if (!coordValid(d)) return 0;
    struct coord dr = getRightDirection(d.x, d.y);
    if (!coordValid(dr)) return 0;
    return !MAX7219_GetXY(&lc, addr, dr.x, dr.y);
}

// ==================== MOVE FUNCTIONS ====================
static void goDown(uint8_t addr, uint8_t x, uint8_t y) {
    struct coord next = getDownDirection(x, y);
    MAX7219_SetXY(&lc, addr, x, y, 0);
    MAX7219_SetXY(&lc, addr, next.x, next.y, 1);
}

static void goDownLeft(uint8_t addr, uint8_t x, uint8_t y) {
    struct coord d  = getDownDirection(x, y);
    struct coord dl = getLeftDirection(d.x, d.y);
    MAX7219_SetXY(&lc, addr, x, y, 0);
    MAX7219_SetXY(&lc, addr, dl.x, dl.y, 1);
}

static void goDownRight(uint8_t addr, uint8_t x, uint8_t y) {
    struct coord d  = getDownDirection(x, y);
    struct coord dr = getRightDirection(d.x, d.y);
    MAX7219_SetXY(&lc, addr, x, y, 0);
    MAX7219_SetXY(&lc, addr, dr.x, dr.y, 1);
}

// ==================== CORE PARTICLE PHYSICS ====================
// Sand physics:
//   1. Try to fall straight down
//   2. Try diagonal-down-left or diagonal-down-right (randomly ordered)
//   3. Otherwise don't move (particle is settled)
uint8_t moveParticle(uint8_t addr, uint8_t x, uint8_t y) {
    if (!MAX7219_GetXY(&lc, addr, x, y)) return 0;

    // 1. Straight down
    if (canGoDown(addr, x, y)) {
        goDown(addr, x, y);
        return 1;
    }

    // 2. Diagonal slides (real sand behaviour)
    uint8_t dl = canGoDownLeft(addr, x, y);
    uint8_t dr = canGoDownRight(addr, x, y);

    if (dl && dr) {
        if (rand() % 2) goDownLeft(addr, x, y);
        else            goDownRight(addr, x, y);
        return 1;
    }
    if (dl) { goDownLeft(addr, x, y);  return 1; }
    if (dr) { goDownRight(addr, x, y); return 1; }

    return 0;  // settled
}

// ==================== MATRIX SCAN ====================
// We scan from gravity-bottom to gravity-top so settled particles
// don't block falling ones in the same frame.
uint8_t updateMatrix(void) {
    uint8_t somethingMoved = 0;

    // For each gravity direction, iterate rows from bottom→top
    // gravity=0:   bottom = y=7, scan y from 7 down to 0
    // gravity=180: bottom = y=0, scan y from 0 up to 7
    // gravity=90:  bottom = x=0, scan x from 0 up to 7
    // gravity=270: bottom = x=7, scan x from 7 down to 0

    for (int8_t major = 7; major >= 0; major--) {
        uint8_t dir = rand() % 2;  // randomise left/right order each row
        for (int8_t minor = 0; minor < 8; minor++) {
            uint8_t col = dir ? (uint8_t)minor : (uint8_t)(7 - minor);
            uint8_t x, y;

            switch (gravity) {
                case 90:  x = (uint8_t)major;     y = col;            break;
                case 180: x = col;                y = (uint8_t)(7 - major); break;
                case 270: x = (uint8_t)(7 - major); y = col;          break;
                default:  x = col;                y = (uint8_t)major; break; // gravity=0
            }

            if (moveParticle(MATRIX_B, x, y)) somethingMoved = 1;
            if (moveParticle(MATRIX_A, x, y)) somethingMoved = 1;
        }
    }
    return somethingMoved;
}

// ==================== HELPERS ====================
uint32_t millis(void) {
    return HAL_GetTick();
}

long getDelayDrop(void) {
    return (long)delayMinutes + (long)delayHours * 60;
}

// Which matrix is physically on top (sand should start there)
// gravity=0   → sand falls +y → MATRIX_A is top
// gravity=180 → sand falls -y → MATRIX_B is top
// gravity=90  → sand falls -x → MATRIX_A is top
// gravity=270 → sand falls +x → MATRIX_B is top
uint8_t getTopMatrix(void) {
    return (gravity == 180 || gravity == 270) ? MATRIX_B : MATRIX_A;
}

uint8_t getBottomMatrix(void) {
    return (getTopMatrix() == MATRIX_A) ? MATRIX_B : MATRIX_A;
}

// ==================== FILL ====================
// Fill the top matrix from the gravity-bottom edge upward,
// so sand looks like it has settled at the bottom of the top chamber.
void fill(uint8_t addr, uint8_t maxcount) {
    uint8_t count = 0;

    // We fill row by row from the gravity-bottom of the given matrix
    // gravity=0: bottom of top matrix is y=7, fill upward
    // gravity=180: bottom is y=0, fill downward (y increasing → away from bottom)
    // gravity=90:  bottom is x=0, fill rightward
    // gravity=270: bottom is x=7, fill leftward

    for (int8_t major = 7; major >= 0 && count < maxcount; major--) {
        for (int8_t minor = 0; minor < 8 && count < maxcount; minor++) {
            uint8_t x, y;
            switch (gravity) {
                case 90:  x = (uint8_t)major;       y = (uint8_t)minor;       break;
                case 180: x = (uint8_t)minor;        y = (uint8_t)(7 - major); break;
                case 270: x = (uint8_t)(7 - major);  y = (uint8_t)minor;       break;
                default:  x = (uint8_t)minor;        y = (uint8_t)major;       break;
            }
            MAX7219_SetXY(&lc, addr, x, y, 1);
            count++;
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

// ==================== RESET ====================
void resetTime(void) {
    MAX7219_ClearDisplay(&lc, MATRIX_A);
    MAX7219_ClearDisplay(&lc, MATRIX_B);
    fill(getTopMatrix(), 60);
    drop_delay.start    = HAL_GetTick();
    drop_delay.interval = getDelayDrop() * 1000UL;
    alarmWentOff = false;
}

// ==================== NECK PIXEL COORDINATES ====================
// The "neck" is the single pixel that connects the two matrices.
// In physical space, the two 8x8 matrices sit one above the other.
// The bottom-most pixel of the top matrix feeds into the top-most pixel
// of the bottom matrix.  Those raw pixel positions depend on gravity.
//
//   gravity=0:   top-matrix bottom row = y=7, centre x=3 or 4
//                bottom-matrix top row = y=0, centre x=3 or 4
//   gravity=180: top-matrix bottom row = y=0, centre x=3 or 4
//                bottom-matrix top row = y=7, centre x=3 or 4
//   gravity=90:  top-matrix bottom col = x=0, centre y=3 or 4
//                bottom-matrix top col = x=7, centre y=3 or 4
//   gravity=270: top-matrix bottom col = x=7, centre y=3 or 4
//                bottom-matrix top col = x=0, centre y=3 or 4
//
// We use a single pixel at the centre of the neck for simplicity,
// matching what the hardware hourglass neck physically does.

typedef struct { uint8_t x; uint8_t y; } NeckPixel;

static NeckPixel getNeckTop(void) {   // pixel in top matrix that drains out
    NeckPixel p;
    switch (gravity) {
        case 90:  p.x = 0; p.y = 3; break;
        case 180: p.x = 3; p.y = 0; break;
        case 270: p.x = 7; p.y = 3; break;
        default:  p.x = 3; p.y = 7; break;  // gravity=0
    }
    return p;
}

static NeckPixel getNeckBottom(void) {  // pixel in bottom matrix that receives
    NeckPixel p;
    switch (gravity) {
        case 90:  p.x = 7; p.y = 3; break;
        case 180: p.x = 3; p.y = 7; break;
        case 270: p.x = 0; p.y = 3; break;
        default:  p.x = 3; p.y = 0; break;  // gravity=0
    }
    return p;
}

// ==================== DROP PARTICLE (NECK TRANSFER) ====================
// Each tick: if neck pixel of top matrix is occupied, move it to bottom matrix.
uint8_t dropParticle(void) {
    if (HAL_GetTick() - drop_delay.start < drop_delay.interval) return 0;

    drop_delay.start    = HAL_GetTick();
    drop_delay.interval = getDelayDrop() * 1000UL;

    uint8_t top = getTopMatrix();
    uint8_t bot = getBottomMatrix();

    NeckPixel nt = getNeckTop();
    NeckPixel nb = getNeckBottom();

    uint8_t top_has = MAX7219_GetXY(&lc, top, nt.x, nt.y);
    uint8_t bot_has = MAX7219_GetXY(&lc, bot, nb.x, nb.y);

    // Only transfer if top has a grain there and bottom neck is free
    if (top_has && !bot_has) {
        MAX7219_SetXY(&lc, top, nt.x, nt.y, 0);
        MAX7219_SetXY(&lc, bot, nb.x, nb.y, 1);

        // Short beep on each grain transfer
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
        HAL_Delay(5);
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
        return 1;
    }
    return 0;
}

// ==================== ALARM ====================
void alarm(void) {
    for (int i = 0; i < 5; i++) {
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
        HAL_Delay(200);
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
        HAL_Delay(800);
    }
}
