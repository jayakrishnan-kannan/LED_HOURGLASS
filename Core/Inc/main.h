/*
 * main.h  —  LED Diamond Hourglass, STM32F103 Blue Pill
 *
 * ════════════════════════════════════════════════════════════════════
 * PHYSICAL LAYOUT
 * ════════════════════════════════════════════════════════════════════
 *
 *  Two 8×8 LED matrices mounted as diamonds (45° rotated), stacked
 *  tip-to-tip, one above the other to form an hourglass of two rhombuses.
 *  Matrix A and Matrix B are 180° opposite each other on the PCB.
 *
 *                    *               ← Matrix A top tip    (logical y=0, 1 pixel wide)
 *                  * * *
 *                * * * * *
 *              * * * * * * *
 *            * * * * * * * * *       ← widest row (logical y=4, 9 pixels... capped at 8)
 *              * * * * * * *
 *                * * * * *
 *                  * * *
 *                    *               ← Matrix A bottom tip (logical y=7, NECK EXIT)
 *                    *               ← Matrix B top tip    (logical y=0, NECK ENTRY)
 *                  * * *
 *                * * * * *
 *              * * * * * * *
 *            * * * * * * * * *
 *              * * * * * * *
 *                * * * * *
 *                  * * *
 *                    *               ← Matrix B bottom tip (logical y=7)
 *
 * ════════════════════════════════════════════════════════════════════
 * LOGICAL COORDINATE SYSTEM  (physics engine space)
 * ════════════════════════════════════════════════════════════════════
 *
 *  The physics engine works in a DIAMOND logical space for each matrix:
 *
 *    Row y=0:  1 pixel  wide  (top tip)
 *    Row y=1:  3 pixels wide
 *    Row y=2:  5 pixels wide
 *    Row y=3:  7 pixels wide
 *    Row y=4:  8 pixels wide  (widest — the full 8-LED row at the equator)
 *    Row y=5:  7 pixels wide
 *    Row y=6:  5 pixels wide
 *    Row y=7:  1 pixel  wide  (bottom tip = neck)
 *
 *  Within each row, x runs 0…(row_width-1), centred.
 *  Sand falls from y=0 toward y=7 (gravity=0 normal orientation).
 *
 *  This coordinate system is SEPARATE from the 8×8 hardware grid.
 *  The fill and physics functions use diamond_row_width(y) and
 *  diamond_to_hw(y, x) to map to actual (hw_x, hw_y) hardware pixels.
 *
 * ════════════════════════════════════════════════════════════════════
 * ORIENTATION TUNING  (one place to change)
 * ════════════════════════════════════════════════════════════════════
 *
 *  In main.c, after MAX7219_Init:
 *
 *    lc.rotation_a = ROT_A;   ← hardware rotation for Matrix A
 *    lc.rotation_b = ROT_B;   ← hardware rotation for Matrix B
 *
 *  ROT_B should always be (ROT_A + 180) % 360 because the two boards
 *  are mounted 180° opposite each other.
 *
 *  Start with ROT_A=0, ROT_B=180.  If the top rhombus appears upside-
 *  down, change ROT_A to 180 and ROT_B to 0.  If it appears sideways,
 *  use ROT_A=90 / ROT_B=270 or ROT_A=270 / ROT_B=90.
 *
 *  #define ROT_A and ROT_B below to set the values.
 */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stdlib.h>
#include "max7219.h"
#include "lis3dh.h"

/* ── Hardware orientation (tune these two values only) ───────────── */
#define ROT_A   0     /* rotation_a for Matrix A — try 0, 90, 180, 270 */
#define ROT_B   90   /* rotation_b for Matrix B — always ROT_A + 180   */

/* ── Pins ────────────────────────────────────────────────────────── */
#define CS_PORT         GPIOA
#define CS_PIN          GPIO_PIN_4
#define BUZZER_PORT     GPIOB
#define BUZZER_PIN      GPIO_PIN_0
#define RESET_BTN_PORT  GPIOB
#define RESET_BTN_PIN   GPIO_PIN_1   /* momentary button to GND, pull-up enabled */
#define LED_PORT        GPIOC
#define LED_PIN         GPIO_PIN_13  /* active LOW */

/* ── Matrix addresses ────────────────────────────────────────────── */
#define MATRIX_A        0u   /* top rhombus    */
#define MATRIX_B        1u   /* bottom rhombus */

/* ── Physics constants ───────────────────────────────────────────── */
/*
 * SAND_GRAINS: total grains in the hourglass.
 * Diamond shape of an 8×8 matrix has:
 *   rows 0..7 → widths 1,3,5,7,8,7,5,3,1 → but we use rows 0..7
 *   practical capacity with nice fill: use 30 (half the visible area)
 * You can increase to 36 (fills roughly the top triangle of each matrix).
 */
#define SAND_GRAINS     30u
#define DELAY_FRAME_MS  80u
#define DEFAULT_HOURS   0u
#define DEFAULT_MINUTES 1u

/* ── Reset thresholds ────────────────────────────────────────────── */
#define RESET_HOLD_MS   1500u   /* button hold time for hard reset      */
#define SHAKE_THRESHOLD 2000    /* accel magnitude threshold (counts)   */
#define SHAKE_COUNT     5       /* consecutive frames to trigger shake  */

/* ── Non-blocking delay ──────────────────────────────────────────── */
typedef struct {
    uint32_t start;
    uint32_t interval;
} NonBlockDelay_t;

/* ── Globals ─────────────────────────────────────────────────────── */
extern Max7219_HandleTypeDef  lc;
extern LIS3DH_HandleTypeDef   accel;
extern SPI_HandleTypeDef      hspi1;
extern I2C_HandleTypeDef      hi2c1;

extern uint8_t          delayHours;
extern uint8_t          delayMinutes;
extern int              gravity;
extern bool             alarmWentOff;
extern NonBlockDelay_t  drop_delay;

/* ── Physics API ─────────────────────────────────────────────────── */
uint32_t millis(void);
void     hourglass_reset(void);
void     hourglass_flip(void);
uint8_t  hourglass_update(void);
uint8_t  hourglass_drop(void);
uint8_t  hourglass_count(uint8_t addr);
uint8_t  hourglass_top_matrix(void);
void     alarm_trigger(void);

void Error_Handler(void);

#ifdef __cplusplus
}
#endif
#endif /* __MAIN_H */
