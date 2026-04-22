/*
 * lis3dh.c
 */
#include "lis3dh.h"

static void reg_write(LIS3DH_HandleTypeDef *dev, uint8_t reg, uint8_t val)
{ HAL_I2C_Mem_Write(dev->hi2c, LIS3DH_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 100); }

static uint8_t reg_read(LIS3DH_HandleTypeDef *dev, uint8_t reg)
{ uint8_t v=0; HAL_I2C_Mem_Read(dev->hi2c, LIS3DH_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &v, 1, 100); return v; }

static int iabs(int v) { return v < 0 ? -v : v; }

uint8_t LIS3DH_Init(LIS3DH_HandleTypeDef *dev, I2C_HandleTypeDef *hi2c)
{
    dev->hi2c = hi2c;
    dev->raw_x = dev->raw_y = dev->raw_z = 0;
    dev->gravity = 0;
    if (reg_read(dev, LIS3DH_WHO_AM_I_REG) != LIS3DH_WHO_AM_I_VAL) return 0;
    reg_write(dev, LIS3DH_CTRL_REG1, 0x57); /* 100 Hz, XYZ on */
    reg_write(dev, LIS3DH_CTRL_REG4, 0x08); /* ±2g, 12-bit    */
    return 1;
}

void LIS3DH_ReadRaw(LIS3DH_HandleTypeDef *dev)
{
    uint8_t buf[6];
    HAL_I2C_Mem_Read(dev->hi2c, LIS3DH_I2C_ADDR, LIS3DH_OUT_X_L|0x80,
                     I2C_MEMADD_SIZE_8BIT, buf, 6, 100);
    dev->raw_x = (int16_t)((buf[1]<<8)|buf[0]) >> 4;
    dev->raw_y = (int16_t)((buf[3]<<8)|buf[2]) >> 4;
    dev->raw_z = (int16_t)((buf[5]<<8)|buf[4]) >> 4;
}

int LIS3DH_GetGravityDirection(LIS3DH_HandleTypeDef *dev)
{
    LIS3DH_ReadRaw(dev);
    int ax = iabs(dev->raw_x), ay = iabs(dev->raw_y), az = iabs(dev->raw_z);
    const int T = 350;
    if (az > ax+T && az > ay+T) return dev->gravity; /* flat — keep last */
    int c = dev->gravity;
    if      (ax > ay+T) c = (dev->raw_x > 0) ? 90  : 270;
    else if (ay > ax+T) c = (dev->raw_y > 0) ? 180 : 0;
    dev->gravity = c;
    return c;
}
