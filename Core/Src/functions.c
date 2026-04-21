/*
 * functions.c  —  Diamond hourglass sand-physics engine
 *
 * ════════════════════════════════════════════════════════════════════
 * THE DIAMOND COORDINATE SYSTEM
 * ════════════════════════════════════════════════════════════════════
 *
 * Each 8×8 matrix is mounted at 45°, forming a rhombus (diamond).
 * The physics engine works in a "diamond logical space":
 *
 *   y = 0  →  1 pixel  wide  (top tip)
 *   y = 1  →  3 pixels wide
 *   y = 2  →  5 pixels wide
 *   y = 3  →  7 pixels wide
 *   y = 4  →  8 pixels wide  (widest row — equator of the diamond)
 *   y = 5  →  7 pixels wide
 *   y = 6  →  5 pixels wide
 *   y = 7  →  1 pixel  wide  (bottom tip = NECK EXIT for top matrix)
 *
 * Row y=4 has 8 pixels because the 45°-rotated 8×8 matrix has its
 * widest diagonal spanning 8 LEDs.  Rows above and below taper by 2
 * pixels per step, except the step from y=3 (7) to y=4 (8) which is +1.
 *
 * Row width formula:
 *   row_width(y) = 2*(y+1) - 1   for y = 0..3  → 1,3,5,7
 *   row_width(4) = 8
 *   row_width(y) = 2*(7-y) + 1   for y = 5..7  → 7,5,3,1
 *
 * Pixels in a row are x = 0 … row_width(y)-1.
 * x=0 is the leftmost pixel of the row.
 *
 * ════════════════════════════════════════════════════════════════════
 * DIAMOND → HARDWARE PIXEL MAPPING
 * ════════════════════════════════════════════════════════════════════
 *
 * A 45°-rotated 8×8 grid maps its diagonals as rows:
 *
 *   Hardware (hw_x, hw_y) for diamond (dy, dx):
 *
 *   The diamond's top tip is the hardware pixel at the top-right corner
 *   if the board is wired so row 0 = top and col 0 = left.
 *   With a 45° rotation:
 *     - Diamond row dy maps to the antidiagonal hw_x + hw_y = constant
 *     - Within that antidiagonal, dx indexes left-to-right
 *
 *   Concretely, for a matrix with its top tip at hardware (0, 0)
 *   and rotated so the diamond fits:
 *
 *   dy=0 (tip):    hw (3,0)  — but this assumes origin at the tip
 *
 *   Instead of this complex mapping, we use the per-matrix rotation
 *   in the MAX7219 driver to physically orient each board, and then
 *   work in the simple (col, row) space the driver expects.
 *
 *   With the 45° physical mounting, the hardware "rows" (as seen by
 *   the MAX7219 row registers) correspond to the diagonal slices of
 *   the diamond visible shape.  We define:
 *
 *     hw_row = dy                        (diamond row = hardware row)
 *     hw_col = dx + offset(dy)           (offset centres the row)
 *
 *   where offset(dy) = number of missing pixels on the left.
 *
 *   offset(dy):
 *     dy=0: offset=3  (tip at col 3)
 *     dy=1: offset=2  (cols 2,3,4)
 *     dy=2: offset=1  (cols 1,2,3,4,5)
 *     dy=3: offset=0  (cols 0..6)
 *     dy=4: offset=0  (cols 0..7)
 *     dy=5: offset=0  (cols 0..6)
 *     dy=6: offset=1  (cols 1..5)
 *     dy=7: offset=3  (col 3)
 *
 *   This mapping is implemented in diamond_to_hw() below.
 *
 *   The MAX7219 driver then applies rotation_a / rotation_b to
 *   convert these (hw_col, hw_row) → final physical LED address.
 *
 * ════════════════════════════════════════════════════════════════════
 * NECK PIXEL
 * ════════════════════════════════════════════════════════════════════
 *
 *   Matrix A bottom tip: diamond (dy=7, dx=0) → hw (col=3, row=7)
 *   Matrix B top tip:    diamond (dy=0, dx=0) → hw (col=3, row=0)
 *
 *   Both are the single-pixel tips at the centre of each board.
 *   The driver rotation maps these to the correct physical LED.
 *
 * ════════════════════════════════════════════════════════════════════
 * SAND PHYSICS IN DIAMOND SPACE
 * ════════════════════════════════════════════════════════════════════
 *
 *   Gravity=0 (upright):
 *     "Down" = increasing dy.
 *     From (dy, dx), a grain can move to:
 *       Straight down:   (dy+1, dx')   where dx' maps to same x position
 *       Diagonal left:   (dy+1, dx'-1) if in bounds
 *       Diagonal right:  (dy+1, dx'+1) if in bounds
 *
 *   The row width changes between rows, so "dx'" is the pixel in row dy+1
 *   that is directly below dx in row dy.  Because of the trapezoidal
 *   shape, this alignment is:
 *     In rows 0..3 (widening):  dx in row dy → dx in row dy+1 (same index,
 *                                but one extra pixel on the right each row)
 *                                "straight down" = dx+1 in the wider row
 *                                because the extra pixel is on the left? No —
 *     See alignment derivation below.
 *
 * ════════════════════════════════════════════════════════════════════
 * ALIGNMENT OF DIAMOND ROWS
 * ════════════════════════════════════════════════════════════════════
 *
 *  Visualising the diamond (all positions are hw column of each pixel):
 *
 *  dy=0:           [3]                         1 pixel,  cols: 3
 *  dy=1:         [2,3,4]                       3 pixels, cols: 2..4
 *  dy=2:       [1,2,3,4,5]                     5 pixels, cols: 1..5
 *  dy=3:     [0,1,2,3,4,5,6]                   7 pixels, cols: 0..6
 *  dy=4:   [0,1,2,3,4,5,6,7]                   8 pixels, cols: 0..7
 *  dy=5:     [0,1,2,3,4,5,6]                   7 pixels, cols: 0..6
 *  dy=6:       [1,2,3,4,5]                     5 pixels, cols: 1..5
 *  dy=7:           [3]                         1 pixel,  cols: 3
 *
 *  offset[] = {3, 2, 1, 0, 0, 0, 1, 3}
 *
 *  For a grain at diamond position (dy, dx):
 *    hw_col = dx + offset[dy]
 *
 *  The grain directly below it in row dy+1 has the SAME hw_col:
 *    dx_below = hw_col - offset[dy+1]
 *             = dx + offset[dy] - offset[dy+1]
 *
 *  So "move straight down" = move to (dy+1, dx + offset[dy] - offset[dy+1])
 *  Diagonal left  = (dy+1, dx + offset[dy] - offset[dy+1] - 1)
 *  Diagonal right = (dy+1, dx + offset[dy] - offset[dy+1] + 1)
 *
 *  All must be checked: 0 <= dx_new < row_width(dy+1)
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

/* ================================================================== */
/* SECTION 1 — Diamond geometry tables                                 */
/* ================================================================== */

/*
 * row_width[dy] = number of pixels in diamond row dy.
 *
 *   dy: 0  1  2  3  4  5  6  7
 *   w:  1  3  5  7  8  7  5  3  1
 *
 * Note: there are 9 rows in a pure diamond but our 8×8 hardware only
 * has 8 rows.  We drop the widest centre row from 9 to 8 by treating
 * dy=4 as the 8-pixel equator.  Total pixels = 1+3+5+7+8+7+5+3+1 but
 * using 8 rows: 1+3+5+7+8+7+5+3 = 39 — but we only use dy 0..7 giving
 * widths: 1,3,5,7,8,7,5,1 = 37.  Use SAND_GRAINS ≤ 30 for a nice fill.
 *
 * Wait — 8 rows means dy 0..7.  Let's define:
 *   dy=0: 1  (top tip)
 *   dy=1: 3
 *   dy=2: 5
 *   dy=3: 7
 *   dy=4: 8  (equator, widest)
 *   dy=5: 7
 *   dy=6: 5
 *   dy=7: 1  (bottom tip = neck)
 */
static const uint8_t ROW_WIDTH[8] = { 1, 3, 5, 7, 8, 7, 5, 1 };

/*
 * ROW_OFFSET[dy] = hardware column of the leftmost pixel in row dy.
 * hw_col = dx + ROW_OFFSET[dy]
 */
static const uint8_t ROW_OFFSET[8] = { 3, 2, 1, 0, 0, 0, 1, 3 };

/* Total pixels in the diamond (sum of ROW_WIDTH) = 37 */
#define DIAMOND_TOTAL  10u

/* ── Diamond validity check ─────────────────────────────────────── */
static uint8_t diamond_valid(int8_t dy, int8_t dx)
{
    if (dy < 0 || dy > 7) return 0;
    if (dx < 0 || dx >= (int8_t)ROW_WIDTH[(uint8_t)dy]) return 0;
    return 1;
}

/* ── Map diamond (dy, dx) → hardware (hw_x=col, hw_y=row) ─────────
 * The diamond row dy IS the hardware row.
 * The hardware column = dx + ROW_OFFSET[dy].
 * MAX7219_SetXY(addr, x=hw_col, y=hw_row) then the driver's per-matrix
 * rotation maps to the physical LED.
 */
static void diamond_to_hw(uint8_t dy, uint8_t dx,
                           uint8_t *hw_x, uint8_t *hw_y)
{
    *hw_x = dx + ROW_OFFSET[dy];   /* column */
    *hw_y = dy;                     /* row    */
}

/* ── Read / write a diamond pixel via the MAX7219 driver ─────────── */
static void diamond_set(uint8_t addr, int8_t dy, int8_t dx, uint8_t on)
{
    uint8_t hx, hy;
    diamond_to_hw((uint8_t)dy, (uint8_t)dx, &hx, &hy);
    MAX7219_SetXY(&lc, addr, hx, hy, on);
}

static uint8_t diamond_get(uint8_t addr, int8_t dy, int8_t dx)
{
    uint8_t hx, hy;
    diamond_to_hw((uint8_t)dy, (uint8_t)dx, &hx, &hy);
    return MAX7219_GetXY(&lc, addr, hx, hy);
}

/* ── Is a diamond cell empty? ────────────────────────────────────── */
static uint8_t cell_empty(uint8_t addr, int8_t dy, int8_t dx)
{
    if (!diamond_valid(dy, dx)) return 0;
    return diamond_get(addr, dy, dx) == 0;
}

/* ── Move grain from (fdy,fdx) to (tdy,tdx) ─────────────────────── */
static void move_grain(uint8_t addr,
                       int8_t fdy, int8_t fdx,
                       int8_t tdy, int8_t tdx)
{
    diamond_set(addr, fdy, fdx, 0);
    diamond_set(addr, tdy, tdx, 1);
}

/* ================================================================== */
/* SECTION 2 — Gravity direction in diamond space                      */
/*                                                                      */
/*  gravity=0  : down = +dy  (normal, sand falls toward neck at dy=7)  */
/*  gravity=180: down = -dy  (upside-down, sand falls toward top tip)  */
/*  gravity=90 : down mapped to lateral — not natural for diamond,     */
/*               treat as gravity=0 (sand still goes to tip)           */
/*  gravity=270: same                                                   */
/*                                                                      */
/*  For 90°/270° sideways tilt the diamond has no natural sideways     */
/*  gravity so we keep sand falling toward the current bottom tip.     */
/*  The display rotation handles the visual orientation.               */
/* ================================================================== */

/* Returns +1 if down = +dy, -1 if down = -dy */
static int8_t dy_direction(void)
{
    return (gravity == 180) ? -1 : +1;
}

/* ================================================================== */
/* SECTION 3 — Single-particle physics in diamond space                */
/*                                                                      */
/*  For gravity=0 (+dy direction):                                      */
/*  A grain at (dy, dx) tries:                                          */
/*    1. Straight down:  (dy+1, dx + ROW_OFFSET[dy] - ROW_OFFSET[dy+1])*/
/*    2. Diag left:   same dx_below - 1                                 */
/*    3. Diag right:  same dx_below + 1                                 */
/*                                                                      */
/*  The "dx_below" formula preserves the physical column position so    */
/*  the grain falls straight down visually.                             */
/* ================================================================== */

static uint8_t step_particle(uint8_t addr, int8_t dy, int8_t dx)
{
    if (!diamond_get(addr, dy, dx)) return 0;   /* cell empty */

    int8_t dir   = dy_direction();
    int8_t dy_to = dy + dir;
    if (dy_to < 0 || dy_to > 7) return 0;   /* at the tip — can't go further */

    /*
     * dx_below: the dx index in row dy_to that is directly below/above dx.
     *
     * Physical column of current grain: hw_col = dx + ROW_OFFSET[dy]
     * dx in target row:                 dx_to  = hw_col - ROW_OFFSET[dy_to]
     *                                           = dx + ROW_OFFSET[dy] - ROW_OFFSET[dy_to]
     */
    int8_t col_shift = (int8_t)ROW_OFFSET[(uint8_t)dy]
                     - (int8_t)ROW_OFFSET[(uint8_t)dy_to];
    int8_t dx_to = dx + col_shift;

    /* 1. Straight down */
    if (cell_empty(addr, dy_to, dx_to)) {
        move_grain(addr, dy, dx, dy_to, dx_to);
        return 1;
    }

    /* 2+3. Diagonals — randomly ordered */
    int8_t dx_left  = dx_to - 1;
    int8_t dx_right = dx_to + 1;
    uint8_t can_l = cell_empty(addr, dy_to, dx_left);
    uint8_t can_r = cell_empty(addr, dy_to, dx_right);

    if (can_l && can_r) {
        if (rand() & 1) move_grain(addr, dy, dx, dy_to, dx_left);
        else            move_grain(addr, dy, dx, dy_to, dx_right);
        return 1;
    }
    if (can_l) { move_grain(addr, dy, dx, dy_to, dx_left);  return 1; }
    if (can_r) { move_grain(addr, dy, dx, dy_to, dx_right); return 1; }

    return 0;   /* settled */
}

/* ================================================================== */
/* SECTION 4 — Matrix scan                                             */
/*                                                                      */
/*  Scan from the bottom row (dy=7) toward top (dy=0) when falling +dy.*/
/*  This ensures a grain lands in an already-processed cell and does   */
/*  not move twice in the same frame.                                  */
/* ================================================================== */

uint8_t hourglass_update(void)
{
    uint8_t moved = 0;
    int8_t  dir   = dy_direction();   /* +1 or -1 */

    /*
     * Start scan at the BOTTOM (gravity direction), go toward TOP.
     * For dir=+1: dy from 7→0  (bottom first = row 7 first)
     * For dir=-1: dy from 0→7  (bottom first = row 0 first since dir=-1)
     */
    int8_t dy_start = (dir == +1) ? 7 : 0;
    int8_t dy_end   = (dir == +1) ? 0 : 7;

    for (int8_t dy = dy_start;
         (dir == +1) ? (dy >= dy_end) : (dy <= dy_end);
         dy -= dir)
    {
        uint8_t w    = ROW_WIDTH[(uint8_t)dy];
        uint8_t flip = (uint8_t)(rand() & 1);   /* randomise left-right order */

        for (uint8_t m = 0; m < w; m++) {
            int8_t dx = (int8_t)(flip ? m : (w - 1 - m));
            /* Bottom matrix processes before top for correct settling */
            if (step_particle(MATRIX_B, dy, dx)) moved = 1;
            if (step_particle(MATRIX_A, dy, dx)) moved = 1;
        }
    }
    return moved;
}

/* ================================================================== */
/* SECTION 5 — Neck pixel in diamond space                             */
/*                                                                      */
/*  gravity=0:   A exit = bottom tip (dy=7, dx=0)                      */
/*               B entry = top tip   (dy=0, dx=0)                      */
/*  gravity=180: A exit = top tip    (dy=0, dx=0)                      */
/*               B entry = bottom tip(dy=7, dx=0)                      */
/* ================================================================== */

typedef struct { int8_t dy; int8_t dx; } DiamondPixel;

static DiamondPixel neck_exit(void)
{
    return (gravity == 180) ? (DiamondPixel){0, 0} : (DiamondPixel){7, 0};
}

static DiamondPixel neck_entry(void)
{
    return (gravity == 180) ? (DiamondPixel){7, 0} : (DiamondPixel){0, 0};
}

/* ================================================================== */
/* SECTION 6 — Timer                                                   */
/* ================================================================== */

uint32_t millis(void) { return HAL_GetTick(); }

static uint32_t drop_interval_ms(void)
{
    uint32_t total_ms = ((uint32_t)delayHours * 60UL
                       + (uint32_t)delayMinutes) * 60000UL;
    if (total_ms == 0) total_ms = 60000UL;
    return total_ms / (uint32_t)SAND_GRAINS;
}

uint8_t hourglass_top_matrix(void)
{
    return (gravity == 180) ? MATRIX_B : MATRIX_A;
}

static uint8_t hourglass_bottom_matrix(void)
{
    return (hourglass_top_matrix() == MATRIX_A) ? MATRIX_B : MATRIX_A;
}

/* ================================================================== */
/* SECTION 7 — Fill  (initial grain placement)                         */
/*                                                                      */
/*  Fill the top matrix from the TOP TIP downward, row by row,         */
/*  placing grains from the tip (dy=0) toward the neck (dy=7).         */
/*                                                                      */
/*  This gives the "full upper chamber" look: the rhombus is filled    */
/*  from the top tip down to a horizontal line.                        */
/*                                                                      */
/*  For gravity=0 (A on top, sand falls +dy):                          */
/*    Fill from dy=0 down.                                              */
/*  For gravity=180 (B on top, sand falls -dy):                        */
/*    Fill from dy=7 up (bottom tip of B is now the physical top).     */
/* ================================================================== */

static void fill_diamond(uint8_t addr, uint8_t count)
{
    uint8_t placed = 0;
    int8_t  dir    = dy_direction();   /* +1 = fill from tip (dy=0) down */
                                        /* -1 = fill from tip (dy=7) up  */

    /* Start at the gravity-top tip, fill toward gravity-bottom */
    int8_t dy_start = (dir == +1) ? 0 : 7;

    for (int8_t dy = dy_start;
         (dir == +1) ? (dy <= 7) : (dy >= 0);
         dy += dir)
    {
        uint8_t w = ROW_WIDTH[(uint8_t)dy];
        for (uint8_t dx = 0; dx < w && placed < count; dx++) {
            diamond_set(addr, dy, (int8_t)dx, 1);
            placed++;
        }
    }
}


/* ================================================================== */
/* SECTION 8 — Public API                                              */
/* ================================================================== */

uint8_t hourglass_count(uint8_t addr)
{
    uint8_t c = 0;
    for (uint8_t dy = 0; dy < 8; dy++)
        for (uint8_t dx = 0; dx < ROW_WIDTH[dy]; dx++)
            if (diamond_get(addr, (int8_t)dy, (int8_t)dx)) c++;
    return c;
}

void hourglass_flip(void)
{
    /* Preserve all grain positions; just restart the timer */
    alarmWentOff        = false;
    drop_delay.start    = HAL_GetTick();
    drop_delay.interval = drop_interval_ms();
}

void hourglass_reset(void)
{
    MAX7219_ClearDisplay(&lc, MATRIX_A);
    MAX7219_ClearDisplay(&lc, MATRIX_B);
    fill_diamond(hourglass_top_matrix(), SAND_GRAINS);
//    fill_rhombus_pattern(hourglass_top_matrix(), SAND_GRAINS);
    alarmWentOff        = false;
    drop_delay.start    = HAL_GetTick();
    drop_delay.interval = drop_interval_ms();
}

uint8_t hourglass_drop(void)
{
    if (HAL_GetTick() - drop_delay.start < drop_delay.interval) return 0;
    drop_delay.start    = HAL_GetTick();
    drop_delay.interval = drop_interval_ms();

    uint8_t      top = hourglass_top_matrix();
    uint8_t      bot = hourglass_bottom_matrix();
    DiamondPixel ex  = neck_exit();
    DiamondPixel en  = neck_entry();

    uint8_t top_has = diamond_get(top, ex.dy, ex.dx);
    uint8_t bot_clr = !diamond_get(bot, en.dy, en.dx);

    if (top_has && bot_clr) {
        diamond_set(top, ex.dy, ex.dx, 0);
        diamond_set(bot, en.dy, en.dx, 1);

        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
        HAL_Delay(4);
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
        return 1;
    }
    return 0;
}

void alarm_trigger(void)
{
    for (int i = 0; i < 5; i++) {
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
        HAL_Delay(200);
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
        HAL_Delay(600);
    }
}
