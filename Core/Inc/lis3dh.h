#ifndef LIS3DH_H
#define LIS3DH_H

#include "stm32f1xx_hal.h"
#define LIS3DH_I2C_ADDR     (0x18 << 1)   // Change to (0x19 << 1) if SA0 is connected to VCC

typedef struct {
    I2C_HandleTypeDef *hi2c;
    int16_t raw_x;
    int16_t raw_y;
    int16_t raw_z;
} LIS3DH_HandleTypeDef;

uint8_t LIS3DH_Init(LIS3DH_HandleTypeDef *dev, I2C_HandleTypeDef *hi2c);
void LIS3DH_ReadRaw(LIS3DH_HandleTypeDef *dev);
int LIS3DH_GetGravityDirection(LIS3DH_HandleTypeDef *dev);   // Returns 0, 90, 180, or 270

#endif
