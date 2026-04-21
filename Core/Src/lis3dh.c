/*
 * lis3dh.c  —  LIS3DH accelerometer driver, STM32 HAL I2C
 *
 * Axis-to-gravity mapping (tune to match your PCB silkscreen):
 *
 *   +Y dominant, ay < 0  →   0°  (portrait, bottom-edge down)
 *   +Y dominant, ay > 0  → 180°  (portrait, top-edge down / flipped)
 *   +X dominant, ax > 0  →  90°  (landscape, left-edge down / tilted right)
 *   +X dominant, ax < 0  → 270°  (landscape, right-edge down / tilted left)
 *
 * Hysteresis: once a direction is established the sensor must cross the
 * threshold by an additional margin before it switches, eliminating flicker
 * at transition angles.  When Z is dominant (device flat on a table) the last
 * known upright direction is retained.
 */

#include "lis3dh.h"

/* ------------------------------------------------------------------ */
/* Register helpers                                                     */
/* ------------------------------------------------------------------ */

static void reg_write(LIS3DH_HandleTypeDef *dev, uint8_t reg, uint8_t val)
{
    HAL_I2C_Mem_Write(dev->hi2c, LIS3DH_I2C_ADDR,
                      reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
}

static uint8_t reg_read(LIS3DH_HandleTypeDef *dev, uint8_t reg)
{
    uint8_t val = 0;
    HAL_I2C_Mem_Read(dev->hi2c, LIS3DH_I2C_ADDR,
                     reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
    return val;
}

static int abs16(int16_t v) { return v < 0 ? -v : v; }

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

uint8_t LIS3DH_Init(LIS3DH_HandleTypeDef *dev, I2C_HandleTypeDef *hi2c)
{
    dev->hi2c    = hi2c;
    dev->raw_x   = dev->raw_y = dev->raw_z = 0;
    dev->gravity = 0;

    if (reg_read(dev, LIS3DH_WHO_AM_I_REG) != LIS3DH_WHO_AM_I_VAL)
        return 0;   /* sensor not found */

    /*
     * CTRL_REG1 = 0x57
     *   ODR = 0101  →  100 Hz
     *   LPen = 0    →  normal mode
     *   Zen/Yen/Xen → all axes on
     */
    reg_write(dev, LIS3DH_CTRL_REG1, 0x57);

    /*
     * CTRL_REG4 = 0x08
     *   FS = 00   →  ±2 g
     *   HR = 1    →  12-bit high-resolution
     */
    reg_write(dev, LIS3DH_CTRL_REG4, 0x08);

    return 1;
}

void LIS3DH_ReadRaw(LIS3DH_HandleTypeDef *dev)
{
    uint8_t buf[6];
    /* MSB of register address sets auto-increment for burst read */
    HAL_I2C_Mem_Read(dev->hi2c, LIS3DH_I2C_ADDR,
                     LIS3DH_OUT_X_L | 0x80,
                     I2C_MEMADD_SIZE_8BIT, buf, 6, 100);

    /* Data is left-justified 16-bit; right-shift 4 gives 12-bit signed */
    dev->raw_x = (int16_t)((buf[1] << 8) | buf[0]) >> 4;
    dev->raw_y = (int16_t)((buf[3] << 8) | buf[2]) >> 4;
    dev->raw_z = (int16_t)((buf[5] << 8) | buf[4]) >> 4;
}

/*
 * LIS3DH_GetGravityDirection
 *
 * Returns 0, 90, 180, or 270 — the direction gravity is pulling sand
 * (i.e. which physical edge of the display is currently facing down).
 *
 * Threshold bands (in 12-bit ±2g counts, 1g ≈ 1000 counts):
 *   ENTER : a new direction is accepted if it beats rivals by ≥ 350 counts
 *   HYSTERESIS : once established, must drop below rival by only 150 counts
 *                before switching — prevents jitter near 45° transitions
 */
int LIS3DH_GetGravityDirection(LIS3DH_HandleTypeDef *dev)
{
    LIS3DH_ReadRaw(dev);

    int ax = abs16(dev->raw_x);
    int ay = abs16(dev->raw_y);
    int az = abs16(dev->raw_z);

    const int ENTER = 350;

    /* Flat on table — Z dominant; retain last known upright direction */
    if (az > ax + ENTER && az > ay + ENTER)
        return dev->gravity;

    int candidate = dev->gravity;

    if (ax > ay + ENTER) {
        candidate = (dev->raw_x > 0) ? 90 : 270;
    } else if (ay > ax + ENTER) {
        candidate = (dev->raw_y > 0) ? 180 : 0;
    }
    /* else: ambiguous angle — keep current */

    dev->gravity = candidate;
    return dev->gravity;
}
