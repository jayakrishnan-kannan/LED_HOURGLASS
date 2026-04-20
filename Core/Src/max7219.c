/*
 * max7219.c
 *
 *  Created on: Apr 19, 2026
 *      Author: jayakrishnan
 */


#include "max7219.h"
#include <string.h>

static void MAX7219_Write(Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t opcode, uint8_t data) {
    uint8_t tx[2] = {opcode, data};
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(dev->hspi, tx, 2, 100);
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_SET);
}

void MAX7219_Init(Max7219_HandleTypeDef *dev, SPI_HandleTypeDef *hspi,
                  GPIO_TypeDef *cs_port, uint16_t cs_pin, uint8_t num_devices) {
    dev->hspi = hspi;
    dev->cs_port = cs_port;
    dev->cs_pin = cs_pin;
    dev->num_devices = (num_devices > 8) ? 8 : num_devices;
    dev->rotation = 0;
    memset(dev->status, 0, sizeof(dev->status));

    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);

    for (uint8_t i = 0; i < dev->num_devices; i++) {
        MAX7219_Write(dev, i, OP_DISPLAYTEST, 0);
        MAX7219_Write(dev, i, OP_SCANLIMIT, 7);
        MAX7219_Write(dev, i, OP_DECODEMODE, 0);
        MAX7219_ClearDisplay(dev, i);
        MAX7219_Shutdown(dev, i, 0);        // normal operation
        MAX7219_SetIntensity(dev, i, 1);
    }
}

void MAX7219_SetRotation(Max7219_HandleTypeDef *dev, int rot) {
    dev->rotation = rot % 360;
}

// Simple coordinate transformation (same logic as original)
static void transform_coord(int rotation, uint8_t *x, uint8_t *y) {
    uint8_t tx = *x, ty = *y;
    if (rotation == 90) {
        *x = ty;
        *y = 7 - tx;
    } else if (rotation == 180) {
        *x = 7 - tx;
        *y = 7 - ty;
    } else if (rotation == 270) {
        *x = 7 - ty;
        *y = tx;
    }
    // 0° = no change
}

void MAX7219_SetXY(Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t x, uint8_t y, uint8_t state) {
    transform_coord(dev->rotation, &x, &y);
    MAX7219_SetRawXY(dev, addr, x, y, state);
}

uint8_t MAX7219_GetXY(Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t x, uint8_t y) {
    transform_coord(dev->rotation, &x, &y);
    return MAX7219_GetRawXY(dev, addr, x, y);
}

void MAX7219_SetRawXY(Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t x, uint8_t y, uint8_t state) {
    if (addr >= dev->num_devices || x > 7 || y > 7) return;

    uint8_t offset = addr * 8 + y;
    uint8_t val = (1u << (7 - x));

    if (state)
        dev->status[offset] |= val;
    else
        dev->status[offset] &= ~val;

    MAX7219_Write(dev, addr, y + 1, dev->status[offset]);   // digit = row+1
}

uint8_t MAX7219_GetRawXY(Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t x, uint8_t y) {
    if (addr >= dev->num_devices || x > 7 || y > 7) return 0;
    uint8_t offset = addr * 8 + y;
    uint8_t val = (1u << (7 - x));
    return (dev->status[offset] & val) ? 1 : 0;
}

void MAX7219_InvertXY(Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t x, uint8_t y) {
    uint8_t state = MAX7219_GetXY(dev, addr, x, y);
    MAX7219_SetXY(dev, addr, x, y, !state);
}

void MAX7219_InvertRawXY(Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t x, uint8_t y) {
    uint8_t state = MAX7219_GetRawXY(dev, addr, x, y);
    MAX7219_SetRawXY(dev, addr, x, y, !state);
}

void MAX7219_ClearDisplay(Max7219_HandleTypeDef *dev, uint8_t addr) {
    if (addr >= dev->num_devices) return;
    for (uint8_t i = 0; i < 8; i++) {
        dev->status[addr*8 + i] = 0;
        MAX7219_Write(dev, addr, i + 1, 0);
    }
}

void MAX7219_SetIntensity(Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t intensity) {
    if (addr >= dev->num_devices || intensity > 15) return;
    MAX7219_Write(dev, addr, OP_INTENSITY, intensity);
}

void MAX7219_Shutdown(Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t shutdown) {
    if (addr >= dev->num_devices) return;
    MAX7219_Write(dev, addr, OP_SHUTDOWN, shutdown ? 0 : 1);
}

void MAX7219_Test_BlinkAll(Max7219_HandleTypeDef *dev)
{
    // Test both matrices (addr 0 and 1)
    for (int repeat = 0; repeat < 3; repeat++)      // Blink 3 times
    {
        // === Turn ALL LEDs ON ===
        for (uint8_t addr = 0; addr < 2; addr++) {
            for (uint8_t row = 0; row < 8; row++) {
                MAX7219_Write(dev, addr, row + 1, 0xFF);   // 0xFF = all 8 LEDs in that row ON
                HAL_Delay(80);
            }
        }

        // === Turn ALL LEDs OFF ===
        for (uint8_t addr = 0; addr < 2; addr++) {
            for (uint8_t row = 0; row < 8; row++) {
                MAX7219_Write(dev, addr, row + 1, 0x00);
                HAL_Delay(80);
            }
        }
        HAL_Delay(400);
    }
}
