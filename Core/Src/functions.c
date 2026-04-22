/*
 * functions.c  —  Hourglass sand-physics engine
 *
 * ════════════════════════════════════════════════════════════════════
 * OVERVIEW
 * ════════════════════════════════════════════════════════════════════
 *
 * Both matrices use the FULL 8×8 grid.  The physical diamond shape
 * is produced by the 45° PCB tilt — this file never clips or masks
 * pixels.  All 64 pixels of each matrix are valid positions for sand.
 *
 * ════════════════════════════════════════════════════════════════════
 * COORDINATE SYSTEM & DIRECTIONS
 * ════════════════════════════════════════════════════════════════════
 *
 * MATRIX A (top, addr 0)
 *   x = 0..7 left→right   y = 0..7 bottom-right→top-left
 *   "Gravity down" in A = toward the bottom tip = toward (7,0)
 *     → x increases, y decreases
 *     → on antidiagonals: antidiag k = x+y, sand moves from k=0 toward k=14
 *       but "down" means x+y INCREASES (toward 7+0=7, then toward 7+0... wait)
 *
 *   Let's be concrete with the layout:
 *     Top tip of A:    (0,7)  — x=0, y=7
 *     Bottom tip of A: (7,0)  — x=7, y=0  ← NECK EXIT
 *     Sand starts at top tip (0,7) and must reach (7,0).
 *     Each step "down": x+1, y-1  (moves diagonally down-right on screen)
 *     antidiag value k = (7-y) + x = x + (7-y)... simpler: use k = x - y
 *       at top tip (0,7): k = 0-7 = -7
 *       at bottom tip (7,0): k = 7-0 = 7
 *     "Down" = increasing (x-y). Each antidiag slice has constant (x-y).
 *
 *   Antidiagonal slices for A (k = x - y, sand moves from k=-7 to k=7):
 *     k=-7: (0,7)                                   — 1 pixel
 *     k=-6: (0,6)(1,7)                              — 2 pixels
 *     k=-5: (0,5)(1,6)(2,7)                         — 3 pixels
 *     k=-4: (0,4)(1,5)(2,6)(3,7)                    — 4 pixels
 *     k=-3: (0,3)(1,4)(2,5)(3,6)(4,7)               — 5 pixels
 *     k=-2: (0,2)(1,3)(2,4)(3,5)(4,6)(5,7)          — 6 pixels
 *     k=-1: (0,1)(1,2)(2,3)(3,4)(4,5)(5,6)(6,7)     — 7 pixels
 *     k= 0: (0,0)(1,1)(2,2)(3,3)(4,4)(5,5)(6,6)(7,7)— 8 pixels (widest)
 *     k= 1: (1,0)(2,1)(3,2)(4,3)(5,4)(6,5)(7,6)     — 7 pixels
 *     k= 2: (2,0)(3,1)(4,2)(5,3)(6,4)(7,5)          — 6 pixels
 *     k= 3: (3,0)(4,1)(5,2)(6,3)(7,4)               — 5 pixels
 *     k= 4: (4,0)(5,1)(6,2)(7,3)                    — 4 pixels
 *     k= 5: (5,0)(6,1)(7,2)                         — 3 pixels
 *     k= 6: (6,0)(7,1)                              — 2 pixels
 *     k= 7: (7,0)                                   — 1 pixel  NECK EXIT
 *
 *   Sand physics in A (gravity=0, down = k increases = x++ y--):
 *     From (x,y), to move "down":
 *       straight: (x+1, y-1)  — stays on same visual column of the diamond
 *       slide L:  (x,   y-1)  — slides to left face
 *       slide R:  (x+1, y  )  — slides to right face
 *     All three targets must be in-bounds [0..7] and empty.
 *
 * MATRIX B (bottom, addr 1)
 *   x = 0..7 left→right   y = 0..7 top-left→bottom-right
 *   Top tip of B:    (7,0)  — NECK ENTRY
 *   Bottom tip of B: (0,7)
 *   Sand enters at (7,0) and settles toward (0,7).
 *   "Settling" direction = toward (0,7) = x decreases, y increases
 *   antidiag k = x - y:
 *     at (7,0): k = 7
 *     at (0,7): k = -7
 *   "Down" in B = k decreases (x--, y++).
 *
 *   Sand physics in B (gravity=0, down = k decreases = x-- y++):
 *     From (x,y), to move "down":
 *       straight: (x-1, y+1)
 *       slide L:  (x-1, y  )
 *       slide R:  (x,   y+1)
 *
 * ════════════════════════════════════════════════════════════════════
 * FILL ORDER
 * ════════════════════════════════════════════════════════════════════
 *
 * Fill Matrix A from the top tip down:
 *   Start at k=-7 (pixel (0,7)), fill k=-6,-5,...,7
 *   Within each k, fill all pixels in that antidiagonal.
 *   Stop when SAND_GRAINS placed.
 *
 * Matrix B starts empty; grains arrive via the neck and settle naturally.
 *
 * ════════════════════════════════════════════════════════════════════
 * GRAVITY=180 (device flipped, B now on top)
 * ════════════════════════════════════════════════════════════════════
 *
 * When flipped, B is on top.  Sand in B must fall toward B's bottom tip
 * (0,7).  The existing B physics (x-- y++) is correct for this direction.
 * Sand in A (now bottom) receives grains at A's top tip (0,7).
 *
 * hourglass_top_matrix() returns MATRIX_B when gravity=180.
 * neck_exit() and neck_entry() swap accordingly.
 */

#include "main.h"
#include "max7219.h"
#include <stdlib.h>

/* ── Globals ─────────────────────────────────────────────────────── */
uint8_t         delayHours   = DEFAULT_HOURS;
uint8_t         delayMinutes = DEFAULT_MINUTES;
int             gravity      = 0;
bool            alarmWentOff = false;
NonBlockDelay_t drop_delay;

/* ── Helpers ─────────────────────────────────────────────────────── */
static uint8_t in_bounds(int x, int y)
{ return (x>=0 && x<=7 && y>=0 && y<=7); }

static uint8_t pix_get(uint8_t addr, int x, int y)
{
    if (!in_bounds(x,y)) return 0;
    return MAX7219_GetXY(&lc, addr, (uint8_t)x, (uint8_t)y);
}

static uint8_t pix_empty(uint8_t addr, int x, int y)
{ return in_bounds(x,y) && !pix_get(addr,(uint8_t)x,(uint8_t)y); }

static void pix_move(uint8_t addr, int fx, int fy, int tx, int ty)
{
    MAX7219_SetXY(&lc, addr, (uint8_t)fx, (uint8_t)fy, 0);
    MAX7219_SetXY(&lc, addr, (uint8_t)tx, (uint8_t)ty, 1);
}

/* ================================================================== */
/* SECTION 1 — Physics for Matrix A                                    */
/*                                                                      */
/* "Down" in A = toward (7,0) = x++, y--                              */
/* From (x,y):                                                         */
/*   straight: (x+1, y-1)                                             */
/*   slide L:  (x,   y-1)   [left face of diamond]                    */
/*   slide R:  (x+1, y  )   [right face of diamond]                   */
/* ================================================================== */
static uint8_t step_A(uint8_t x, uint8_t y)
{
    if (!pix_get(MATRIX_A, x, y)) return 0;
    int ix = x, iy = y;

    /* 1. Straight down */
    if (pix_empty(MATRIX_A, ix+1, iy-1)) {
        pix_move(MATRIX_A, ix, iy, ix+1, iy-1);
        return 1;
    }
    /* 2+3. Diagonal slides — randomise order */
    uint8_t cL = pix_empty(MATRIX_A, ix,   iy-1);
    uint8_t cR = pix_empty(MATRIX_A, ix+1, iy  );
    if (cL && cR) {
        if (rand()&1) pix_move(MATRIX_A, ix, iy, ix,   iy-1);
        else          pix_move(MATRIX_A, ix, iy, ix+1, iy  );
        return 1;
    }
    if (cL) { pix_move(MATRIX_A, ix, iy, ix,   iy-1); return 1; }
    if (cR) { pix_move(MATRIX_A, ix, iy, ix+1, iy  ); return 1; }
    return 0;
}

/* ================================================================== */
/* SECTION 2 — Physics for Matrix B                                    */
/*                                                                      */
/* "Down" in B = toward (0,7) = x--, y++                              */
/* From (x,y):                                                         */
/*   straight: (x-1, y+1)                                             */
/*   slide L:  (x-1, y  )   [left face]                               */
/*   slide R:  (x,   y+1)   [right face]                              */
/* ================================================================== */
static uint8_t step_B(uint8_t x, uint8_t y)
{
    if (!pix_get(MATRIX_B, x, y)) return 0;
    int ix = x, iy = y;

    if (pix_empty(MATRIX_B, ix-1, iy+1)) {
        pix_move(MATRIX_B, ix, iy, ix-1, iy+1);
        return 1;
    }
    uint8_t cL = pix_empty(MATRIX_B, ix-1, iy  );
    uint8_t cR = pix_empty(MATRIX_B, ix,   iy+1);
    if (cL && cR) {
        if (rand()&1) pix_move(MATRIX_B, ix, iy, ix-1, iy  );
        else          pix_move(MATRIX_B, ix, iy, ix,   iy+1);
        return 1;
    }
    if (cL) { pix_move(MATRIX_B, ix, iy, ix-1, iy  ); return 1; }
    if (cR) { pix_move(MATRIX_B, ix, iy, ix,   iy+1); return 1; }
    return 0;
}

/* ================================================================== */
/* SECTION 3 — Matrix scan order                                       */
/*                                                                      */
/* Scan bottom-first (highest k first for A, lowest k first for B)    */
/* so settled grains don't block falling ones in the same frame.       */
/*                                                                      */
/* For A (down = x++, y--):                                            */
/*   Scan from k=7 (bottom tip) up to k=-7 (top tip).                 */
/*   k = x - y, so scan pixels with highest (x-y) first.              */
/*   For each k, iterate over all valid (x,y) pairs in that slice.    */
/*                                                                      */
/* For B (down = x--, y++):                                            */
/*   Scan from k=-7 (bottom tip) up to k=7 (top tip).                 */
/*   For each k, iterate pixels with lowest (x-y) first.              */
/* ================================================================== */
uint8_t hourglass_update(void)
{
    uint8_t moved = 0;

    /* ── Matrix A: scan k from 7 (bottom) down to -7 (top) ── */
    for (int k = 7; k >= -7; k--) {
        /* All (x,y) where x-y == k, x in 0..7, y in 0..7 */
        /* x = y+k, y in max(0,-k)..min(7,7-k) */
        int y_min = (k < 0) ? -k : 0;
        int y_max = (k < 0) ? 7  : 7-k;
        /* Randomise within-slice order */
        uint8_t flip = rand()&1;
        for (int yi = 0; yi <= (y_max - y_min); yi++) {
            int y = flip ? (y_min + yi) : (y_max - yi);
            int x = y + k;
            if (x<0||x>7||y<0||y>7) continue;
            if (step_A((uint8_t)x, (uint8_t)y)) moved = 1;
        }
    }

    /* ── Matrix B: scan k from -7 (bottom) up to 7 (top) ── */
    for (int k = -7; k <= 7; k++) {
        int y_min = (k < 0) ? -k : 0;
        int y_max = (k < 0) ? 7  : 7-k;
        uint8_t flip = rand()&1;
        for (int yi = 0; yi <= (y_max - y_min); yi++) {
            int y = flip ? (y_min + yi) : (y_max - yi);
            int x = y + k;
            if (x<0||x>7||y<0||y>7) continue;
            if (step_B((uint8_t)x, (uint8_t)y)) moved = 1;
        }
    }

    return moved;
}

/* ================================================================== */
/* SECTION 4 — Neck pixel & timer                                      */
/* ================================================================== */

uint32_t millis(void) { return HAL_GetTick(); }

static uint32_t drop_interval_ms(void)
{
    uint32_t t = ((uint32_t)delayHours*60 + delayMinutes) * 60000UL;
    if (!t) t = 60000UL;
    return t / SAND_GRAINS;
}

uint8_t hourglass_top_matrix(void)
{ return (gravity==180) ? MATRIX_B : MATRIX_A; }

static uint8_t hourglass_bottom_matrix(void)
{ return (hourglass_top_matrix()==MATRIX_A) ? MATRIX_B : MATRIX_A; }

/*
 * Neck pixels (logical coords passed to MAX7219_GetXY/SetXY):
 *
 * gravity=0:
 *   A exit  = (7,0)  — bottom tip of A
 *   B entry = (7,0)  — top tip of B
 *
 * gravity=180 (flipped):
 *   B exit  = (0,7)  — B's bottom tip is now physically on top
 *   A entry = (0,7)  — A's top tip receives
 *
 * Note: after hourglass_top_matrix() returns the correct source,
 * neck_exit() is for that matrix and neck_entry() is for the other.
 */
typedef struct { uint8_t x; uint8_t y; } Pix;

static Pix neck_exit(void)
{
    /* Exit point of the TOP matrix */
    if (gravity == 180) return (Pix){0,0};  /* B's bottom tip */
    return (Pix){7,7};                       /* A's bottom tip */
}

static Pix neck_entry(void)
{
    /* Entry point of the BOTTOM matrix */
    if (gravity == 180) return (Pix){7,7};  /* A's top tip    */
    return (Pix){0,0};                       /* B's top tip    */
}

uint8_t hourglass_drop(void)
{
    if (HAL_GetTick() - drop_delay.start < drop_delay.interval) return 0;
    drop_delay.start    = HAL_GetTick();
    drop_delay.interval = drop_interval_ms();

    uint8_t top = hourglass_top_matrix();
    uint8_t bot = hourglass_bottom_matrix();
    Pix     ex  = neck_exit();
    Pix     en  = neck_entry();

    if (MAX7219_GetXY(&lc, top, ex.x, ex.y) &&
       !MAX7219_GetXY(&lc, bot, en.x, en.y))
    {
        MAX7219_SetXY(&lc, top, ex.x, ex.y, 0);
        MAX7219_SetXY(&lc, bot, en.x, en.y, 1);
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
        HAL_Delay(4);
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
        return 1;
    }
    return 0;
}

/* ================================================================== */
/* SECTION 5 — Fill                                                    */
/*                                                                      */
/* Fill Matrix A from top tip (0,7) downward through antidiagonals.   */
/* Antidiag k = x-y, fill k=-7,-6,...,7 until SAND_GRAINS placed.    */
/*                                                                      */
/* For gravity=180, fill Matrix B from its top tip (7,0) downward.    */
/* In B "downward" from (7,0) means increasing antidiag k from        */
/* k=7 toward k=-7 (toward (0,7)).                                    */
/* ================================================================== */
static void fill_A(uint8_t count)
{
    uint8_t placed = 0;
    /* Fill A from k=-7 (top tip) to k=7 (bottom tip) */
    for (int k = -7; k <= 7 && placed < count; k++) {
        int y_min = (k<0)?-k:0;
        int y_max = (k<0)?7:7-k;
        for (int y = y_min; y <= y_max && placed < count; y++) {
            int x = y + k;
            if (x<0||x>7) continue;
            MAX7219_SetXY(&lc, MATRIX_A, (uint8_t)x, (uint8_t)y, 1);
            placed++;
        }
    }
}

static void fill_B(uint8_t count)
{
    uint8_t placed = 0;
    /* Fill B from k=7 (top tip (7,0)) toward k=-7 (bottom tip (0,7)) */
    for (int k = 7; k >= -7 && placed < count; k--) {
        int y_min = (k<0)?-k:0;
        int y_max = (k<0)?7:7-k;
        for (int y = y_min; y <= y_max && placed < count; y++) {
            int x = y + k;
            if (x<0||x>7) continue;
            MAX7219_SetXY(&lc, MATRIX_B, (uint8_t)x, (uint8_t)y, 1);
            placed++;
        }
    }
}

/* ================================================================== */
/* SECTION 6 — Public API                                              */
/* ================================================================== */

uint8_t hourglass_count(uint8_t addr)
{
    uint8_t c = 0;
    for (uint8_t y=0; y<8; y++)
        for (uint8_t x=0; x<8; x++)
            if (MAX7219_GetXY(&lc, addr, x, y)) c++;
    return c;
}

void hourglass_flip(void)
{
    /* Preserve grain state, just restart timer */
    alarmWentOff        = false;
    drop_delay.start    = HAL_GetTick();
    drop_delay.interval = drop_interval_ms();
}

void hourglass_reset(void)
{
    MAX7219_ClearDisplay(&lc, MATRIX_A);
    MAX7219_ClearDisplay(&lc, MATRIX_B);
    if (gravity == 180) fill_B(SAND_GRAINS);
    else                fill_A(SAND_GRAINS);
    alarmWentOff        = false;
    drop_delay.start    = HAL_GetTick();
    drop_delay.interval = drop_interval_ms();
}

void alarm_trigger(void)
{
    for (int i=0; i<5; i++) {
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
        HAL_Delay(200);
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
        HAL_Delay(600);
    }
}
