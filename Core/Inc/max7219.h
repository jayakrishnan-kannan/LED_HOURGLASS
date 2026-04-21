/*
 * max7219.h  —  MAX7219 8×8 LED matrix driver, STM32 HAL SPI
 *
 * ════════════════════════════════════════════════════════════════════
 * DIAMOND (45°) MOUNTING — HOW THIS DRIVER HANDLES IT
 * ════════════════════════════════════════════════════════════════════
 *
 * Both LED matrices are physically mounted as diamonds (rotated 45°).
 * The two matrices are also 180° opposite each other on the PCB.
 *
 * This driver supports per-matrix rotation so each matrix can have
 * its own hardware orientation correction independent of the other.
 *
 * Fields in Max7219_HandleTypeDef:
 *
 *   rotation_a   — hardware rotation correction for MATRIX_A (addr 0)
 *   rotation_b   — hardware rotation correction for MATRIX_B (addr 1)
 *
 * Both are applied BEFORE the logical sand coordinate reaches hardware.
 *
 * The physics engine (functions.c) always works in a clean logical
 * space:
 *   x 0..7  left→right
 *   y 0..7  top→bottom   (y=0 = widest row at top of diamond,
 *                          y=7 = single tip pixel at bottom)
 *
 * The driver's per-matrix rotation maps that logical space onto
 * whatever hardware orientation each physical board has.
 *
 * ════════════════════════════════════════════════════════════════════
 * WHAT TO SET FOR THE HOURGLASS DIAMOND LAYOUT
 * ════════════════════════════════════════════════════════════════════
 *
 * Matrix A (top rhombus, addr 0):
 *   Mounted 45° rotated, upright.
 *   Set rotation_a to compensate for the 45° PCB mount.
 *   Because the MAX7219 can only do 0/90/180/270, you physically rotate
 *   the board so one of those four aligns with your wiring, then set
 *   the matching value here.
 *
 * Matrix B (bottom rhombus, addr 1):
 *   Mounted 45° rotated AND 180° flipped relative to A.
 *   Set rotation_b = (rotation_a + 180) % 360.
 *
 * In main.c:
 *   lc.rotation_a = 0;    // adjust to match your actual wiring
 *   lc.rotation_b = 180;  // always 180° different from rotation_a
 *
 * No other file needs changing for orientation.
 */

#ifndef MAX7219_H
#define MAX7219_H

#include "stm32f1xx_hal.h"

/* MAX7219 opcodes */
#define OP_NOOP        0x00
#define OP_DIGIT0      0x01
#define OP_DECODEMODE  0x09
#define OP_INTENSITY   0x0A
#define OP_SCANLIMIT   0x0B
#define OP_SHUTDOWN    0x0C
#define OP_DISPLAYTEST 0x0F

#define MAX7219_NO_INVERT  0xFF

typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef      *cs_port;
    uint16_t           cs_pin;
    uint8_t            num_devices;

    /* Shadow RAM: status[addr*8 + row], bit7=col0 … bit0=col7 */
    uint8_t status[64];

    /*
     * Per-matrix hardware rotation corrections.
     * rotation_a applies to MATRIX_A (addr 0).
     * rotation_b applies to MATRIX_B (addr 1).
     * Valid values: 0, 90, 180, 270.
     *
     * Set these in main.c to match your physical PCB wiring.
     * The two values should differ by 180 for the hourglass layout
     * (since B is mounted 180° opposite A).
     */
    int rotation_a;
    int rotation_b;

    /*
     * inverted_matrix: address of a matrix wired with DATA/CLK swapped
     * such that its pixel rows are reversed.  Set to MAX7219_NO_INVERT
     * if not applicable.  This is separate from rotation — rotation is
     * about the display angle, inversion is about reversed wiring.
     */
    uint8_t inverted_matrix;
} Max7219_HandleTypeDef;

/* Init & config */
void MAX7219_Init        (Max7219_HandleTypeDef *dev, SPI_HandleTypeDef *hspi,
                          GPIO_TypeDef *cs_port, uint16_t cs_pin, uint8_t num_devices);
void MAX7219_SetIntensity (Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t intensity);
void MAX7219_Shutdown     (Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t shutdown);
void MAX7219_ClearDisplay (Max7219_HandleTypeDef *dev, uint8_t addr);

/* Logical pixel access — applies per-matrix rotation + inversion */
void    MAX7219_SetXY    (Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t x, uint8_t y, uint8_t on);
uint8_t MAX7219_GetXY    (Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t x, uint8_t y);
void    MAX7219_InvertXY (Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t x, uint8_t y);

/* Raw pixel access — applies inversion only, no rotation */
void    MAX7219_SetRawXY    (Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t x, uint8_t y, uint8_t on);
uint8_t MAX7219_GetRawXY    (Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t x, uint8_t y);
void    MAX7219_InvertRawXY (Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t x, uint8_t y);

/* Diagnostics */
void MAX7219_Test_BlinkAll(Max7219_HandleTypeDef *dev);

#endif /* MAX7219_H */
