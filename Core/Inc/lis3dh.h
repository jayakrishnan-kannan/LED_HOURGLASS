/*
 * lis3dh.h
 */
#ifndef LIS3DH_H
#define LIS3DH_H

#include "stm32f1xx_hal.h"

//#define LIS3DH_I2C_ADDR     (LIS3DH_Addr)
#define LIS3DH_WHO_AM_I_REG  0x0F
#define LIS3DH_WHO_AM_I_VAL  0x33
#define LIS3DH_CTRL_REG1     0x20
#define LIS3DH_CTRL_REG4     0x23
#define LIS3DH_OUT_X_L       0x28

typedef struct {
    I2C_HandleTypeDef *hi2c;
    int16_t raw_x, raw_y, raw_z;
    int     gravity;
} LIS3DH_HandleTypeDef;

uint8_t LIS3DH_Init              (LIS3DH_HandleTypeDef *dev, I2C_HandleTypeDef *hi2c);
void    LIS3DH_ReadRaw           (LIS3DH_HandleTypeDef *dev);
int     LIS3DH_GetGravityDirection(LIS3DH_HandleTypeDef *dev);

#endif
