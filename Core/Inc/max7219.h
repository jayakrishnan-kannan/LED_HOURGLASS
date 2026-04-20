/*
 * max7219.h
 *
 *  Created on: Apr 19, 2026
 *      Author: jayakrishnan
 */

#ifndef INC_MAX7219_H_
#define INC_MAX7219_H_
#ifndef MAX7219_H
#define MAX7219_H

#include "stm32f1xx_hal.h"   // Change to your series: stm32f1xx_hal.h, stm32f4xx_hal.h, etc.

#define OP_NOOP         0x00
#define OP_DIGIT0       0x01
#define OP_DECODEMODE   0x09
#define OP_INTENSITY    0x0A
#define OP_SCANLIMIT    0x0B
#define OP_SHUTDOWN     0x0C
#define OP_DISPLAYTEST  0x0F

typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    uint8_t num_devices;
    uint8_t status[64];        // 8 bytes per device × 8 devices max
    int rotation;
} Max7219_HandleTypeDef;

void MAX7219_Init(Max7219_HandleTypeDef *dev, SPI_HandleTypeDef *hspi,
                  GPIO_TypeDef *cs_port, uint16_t cs_pin, uint8_t num_devices);

void MAX7219_SetRotation(Max7219_HandleTypeDef *dev, int rot);
void MAX7219_ClearDisplay(Max7219_HandleTypeDef *dev, uint8_t addr);
void MAX7219_SetIntensity(Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t intensity);
void MAX7219_Shutdown(Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t shutdown);

void MAX7219_SetXY(Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t x, uint8_t y, uint8_t state);
uint8_t MAX7219_GetXY(Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t x, uint8_t y);
void MAX7219_InvertXY(Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t x, uint8_t y);
void MAX7219_InvertRawXY(Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t x, uint8_t y);

void MAX7219_SetRawXY(Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t x, uint8_t y, uint8_t state);
uint8_t MAX7219_GetRawXY(Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t x, uint8_t y);
void MAX7219_Test_BlinkAll(Max7219_HandleTypeDef *dev);

#endif



#endif /* INC_MAX7219_H_ */
