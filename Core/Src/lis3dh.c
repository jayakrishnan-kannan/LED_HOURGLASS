/*
 * lis3dh.c
 *
 *  Created on: Apr 19, 2026
 *      Author: jayakrishnan
 */

#include "lis3dh.h"

static void LIS3DH_WriteReg(LIS3DH_HandleTypeDef *dev, uint8_t reg, uint8_t value)
{
    HAL_I2C_Mem_Write(dev->hi2c, LIS3DH_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &value, 1, 100);
}

static uint8_t LIS3DH_ReadReg(LIS3DH_HandleTypeDef *dev, uint8_t reg)
{
    uint8_t value = 0;
    HAL_I2C_Mem_Read(dev->hi2c, LIS3DH_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &value, 1, 100);
    return value;
}

uint8_t LIS3DH_Init(LIS3DH_HandleTypeDef *dev, I2C_HandleTypeDef *hi2c)
{
    dev->hi2c = hi2c;
    dev->raw_x = dev->raw_y = dev->raw_z = 0;

    // Check sensor identity
    if (LIS3DH_ReadReg(dev, 0x0F) != 0x33) {
        return 0;   // Wrong WHO_AM_I → sensor not found
    }

    // CTRL_REG1: 100 Hz data rate, normal mode, enable X/Y/Z axes
    LIS3DH_WriteReg(dev, 0x20, 0x57);

    // CTRL_REG4: ±2g range, high-resolution mode (12-bit)
    LIS3DH_WriteReg(dev, 0x23, 0x08);

    return 1;
}

void LIS3DH_ReadRaw(LIS3DH_HandleTypeDef *dev)
{
    uint8_t buf[6];

    // Read 6 bytes starting from OUT_X_L with auto-increment (bit 7 set)
    HAL_I2C_Mem_Read(dev->hi2c, LIS3DH_I2C_ADDR, 0x28 | 0x80,
                     I2C_MEMADD_SIZE_8BIT, buf, 6, 100);

    dev->raw_x = (int16_t)(buf[0] | (buf[1] << 8)) >> 4;
    dev->raw_y = (int16_t)(buf[2] | (buf[3] << 8)) >> 4;
    dev->raw_z = (int16_t)(buf[4] | (buf[5] << 8)) >> 4;
}

// Improved gravity direction detection (only I2C, no ADC)
int LIS3DH_GetGravityDirection(LIS3DH_HandleTypeDef *dev)
{
    LIS3DH_ReadRaw(dev);

    int16_t ax = dev->raw_x;
    int16_t ay = dev->raw_y;
    int16_t az = dev->raw_z;

    // Use absolute values to find dominant axis (gravity ~ ±1000 to ±1600 in ±2g range)
    int abs_x = ax > 0 ? ax : -ax;
    int abs_y = ay > 0 ? ay : -ay;
    int abs_z = az > 0 ? az : -az;

    // Threshold helps ignore small noise when nearly flat
    const int threshold = 300;

    if (abs_z > abs_x + threshold && abs_z > abs_y + threshold) {
        // Mostly vertical (flat on table) → treat as 0° (Y-down in original logic)
        return 0;
    }
    else if (abs_x > abs_y && abs_x > abs_z) {
        return (ax > 0) ? 90 : 270;     // Tilt right → 90°, left → 270°
    }
    else if (abs_y > abs_x && abs_y > abs_z) {
        return (ay > 0) ? 180 : 0;      // ay positive → upside down (180°), negative → normal (0°)
    }

    // Default / ambiguous case
    return 0;
}
