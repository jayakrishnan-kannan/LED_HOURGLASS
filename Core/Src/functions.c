#include "main.h"
#include "max7219.h"
#include <stdlib.h>

uint8_t         delayHours   = DEFAULT_HOURS;
uint8_t         delayMinutes = DEFAULT_MINUTES;
int             gravity      = 180;
bool            alarmWentOff = false;
NonBlockDelay_t drop_delay;

static uint8_t in_bounds(int x, int y)
{ return (x>=0 && x<=7 && y>=0 && y<=7); }

static uint8_t pix_get(uint8_t addr, int x, int y)
{
    if (!in_bounds(x,y)) return 0;
    return MAX7219_GetXY(&lc, addr, (uint8_t)x, (uint8_t)y);
}

static uint8_t pix_empty(uint8_t addr, int x, int y)
{ return in_bounds(x,y) && !pix_get(addr, x, y); }

static void pix_move(uint8_t addr, int fx, int fy, int tx, int ty)
{
    MAX7219_SetXY(&lc, addr, (uint8_t)fx, (uint8_t)fy, 0);
    MAX7219_SetXY(&lc, addr, (uint8_t)tx, (uint8_t)ty, 1);
}

/*
 * step_toward_07: move grain toward (0,7), x-- y++
 * gravity=180: used for Matrix B (top, falls toward neck (0,7))
 * gravity=0:   used for Matrix A (top, falls toward neck (0,7)??
 *
 * VERIFIED gravity=180 works:
 *   B top   neck=(0,7) -> step_toward_07 ✓
 *   A bot   neck=(0,7), far=(7,0) -> step_toward_70 ✓
 *
 * gravity=0 observed behavior (mirrored):
 *   A top   neck=(7,0) -> grains go AWAY from (7,0) -> step is toward_07 on A
 *   B bot   entry=(7,0)? -> grains settle toward (7,0) -> step is toward_70 on B
 *
 * Therefore for gravity=0: swap step functions AND swap scan orders.
 */

static uint8_t step_toward_07(uint8_t addr, uint8_t x, uint8_t y)
{
    if (!pix_get(addr, x, y)) return 0;
    int ix = x, iy = y;
    if (pix_empty(addr, ix-1, iy+1)) { pix_move(addr, ix, iy, ix-1, iy+1); return 1; }
    uint8_t cL = pix_empty(addr, ix-1, iy  );
    uint8_t cR = pix_empty(addr, ix,   iy+1);
    if (cL && cR) {
        if (rand()&1) pix_move(addr, ix, iy, ix-1, iy  );
        else          pix_move(addr, ix, iy, ix,   iy+1);
        return 1;
    }
    if (cL) { pix_move(addr, ix, iy, ix-1, iy  ); return 1; }
    if (cR) { pix_move(addr, ix, iy, ix,   iy+1); return 1; }
    return 0;
}

static uint8_t step_toward_70(uint8_t addr, uint8_t x, uint8_t y)
{
    if (!pix_get(addr, x, y)) return 0;
    int ix = x, iy = y;
    if (pix_empty(addr, ix+1, iy-1)) { pix_move(addr, ix, iy, ix+1, iy-1); return 1; }
    uint8_t cL = pix_empty(addr, ix,   iy-1);
    uint8_t cR = pix_empty(addr, ix+1, iy  );
    if (cL && cR) {
        if (rand()&1) pix_move(addr, ix, iy, ix,   iy-1);
        else          pix_move(addr, ix, iy, ix+1, iy  );
        return 1;
    }
    if (cL) { pix_move(addr, ix, iy, ix,   iy-1); return 1; }
    if (cR) { pix_move(addr, ix, iy, ix+1, iy  ); return 1; }
    return 0;
}

static void scan_k_neg_to_pos(uint8_t addr,
                               uint8_t (*stepfn)(uint8_t, uint8_t, uint8_t),
                               uint8_t *moved)
{
    for (int k = -7; k <= 7; k++) {
        int y_min = (k < 0) ? -k : 0;
        int y_max = (k < 0) ?  7 : 7-k;
        uint8_t flip = rand() & 1;
        for (int yi = 0; yi <= (y_max - y_min); yi++) {
            int y = flip ? (y_min + yi) : (y_max - yi);
            int x = y + k;
            if (x < 0 || x > 7 || y < 0 || y > 7) continue;
            if (stepfn(addr, (uint8_t)x, (uint8_t)y)) *moved = 1;
        }
    }
}

static void scan_k_pos_to_neg(uint8_t addr,
                               uint8_t (*stepfn)(uint8_t, uint8_t, uint8_t),
                               uint8_t *moved)
{
    for (int k = 7; k >= -7; k--) {
        int y_min = (k < 0) ? -k : 0;
        int y_max = (k < 0) ?  7 : 7-k;
        uint8_t flip = rand() & 1;
        for (int yi = 0; yi <= (y_max - y_min); yi++) {
            int y = flip ? (y_min + yi) : (y_max - yi);
            int x = y + k;
            if (x < 0 || x > 7 || y < 0 || y > 7) continue;
            if (stepfn(addr, (uint8_t)x, (uint8_t)y)) *moved = 1;
        }
    }
}

uint8_t hourglass_update(void)
{
    uint8_t moved = 0;

    if (gravity == 180) {
        /*
         * B on top: falls toward (0,7) = k=-7.
         *   step_toward_07, scan settled-end (k=-7) first = k=-7 to k=+7.
         * A on bot: settles toward (7,0) = k=+7.
         *   step_toward_70, scan settled-end (k=+7) first = k=+7 to k=-7.
         */
        scan_k_neg_to_pos(MATRIX_B, step_toward_07, &moved);
        scan_k_pos_to_neg(MATRIX_A, step_toward_70, &moved);
    } else {
        /*
         * gravity=0: A on top, B on bottom.
         * Observed (hardware): A grains go toward (0,7), B settles toward (7,0).
         * So step directions are SWAPPED vs gravity=180:
         *   A: step_toward_07, settled end = (0,7) = k=-7, scan k=-7 to k=+7.
         *   B: step_toward_70, settled end = (7,0) = k=+7, scan k=+7 to k=-7.
         */
        scan_k_neg_to_pos(MATRIX_A, step_toward_07, &moved);
        scan_k_pos_to_neg(MATRIX_B, step_toward_70, &moved);
    }

    return moved;
}

static uint32_t drop_interval_ms(void)
{
    uint32_t t = ((uint32_t)delayHours * 60 + delayMinutes) * 60000UL;
    if (!t) t = 60000UL;
    return t / SAND_GRAINS;
}

uint8_t hourglass_top_matrix(void)
{ return (gravity == 180) ? MATRIX_B : MATRIX_A; }

static uint8_t hourglass_bottom_matrix(void)
{ return (hourglass_top_matrix() == MATRIX_A) ? MATRIX_B : MATRIX_A; }

typedef struct { uint8_t x; uint8_t y; } Pix;

static Pix neck_exit(void)
{
    /*
     * gravity=180: B top, exits at (0,7).
     * gravity=0:   A top, observed dripping from (0,7) not (7,0).
     *              So exit = (0,7) for both.
     */
    return (Pix){0, 7};
}

static Pix neck_entry(void)
{
    /*
     * gravity=180: A bot, entry at (0,7).
     * gravity=0:   B bot, observed entering at (0,7).
     *              So entry = (0,7) for both.
     */
    return (Pix){0, 7};
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

    if ( MAX7219_GetXY(&lc, top, ex.x, ex.y) &&
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

/*
 * fill_B: gravity=180, B top. Far tip=(7,0)=k=+7. Fill k=+7 toward k=-7.
 * fill_A: gravity=0,   A top. Far tip=(7,0)=k=+7 (mirrors B). Fill k=+7 toward k=-7.
 *   (gravity=0 A far tip is (7,0) because step goes toward_07 = away from (7,0))
 */
static void fill_A(uint8_t count)
{
    uint8_t placed = 0;
    for (int k = 7; k >= -7 && placed < count; k--) {
        int y_min = (k < 0) ? -k : 0;
        int y_max = (k < 0) ?  7 : 7-k;
        for (int y = y_min; y <= y_max && placed < count; y++) {
            int x = y + k;
            if (x < 0 || x > 7) continue;
            MAX7219_SetXY(&lc, MATRIX_A, (uint8_t)x, (uint8_t)y, 1);
            placed++;
        }
    }
}

static void fill_B(uint8_t count)
{
    uint8_t placed = 0;
    for (int k = 7; k >= -7 && placed < count; k--) {
        int y_min = (k < 0) ? -k : 0;
        int y_max = (k < 0) ?  7 : 7-k;
        for (int y = y_min; y <= y_max && placed < count; y++) {
            int x = y + k;
            if (x < 0 || x > 7) continue;
            MAX7219_SetXY(&lc, MATRIX_B, (uint8_t)x, (uint8_t)y, 1);
            placed++;
        }
    }
}

uint8_t hourglass_count(uint8_t addr)
{
    uint8_t c = 0;
    for (uint8_t y = 0; y < 8; y++)
        for (uint8_t x = 0; x < 8; x++)
            if (MAX7219_GetXY(&lc, addr, x, y)) c++;
    return c;
}

void hourglass_flip(void)
{
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
    for (int i = 0; i < 5; i++) {
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
        HAL_Delay(200);
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
        HAL_Delay(600);
    }
}
