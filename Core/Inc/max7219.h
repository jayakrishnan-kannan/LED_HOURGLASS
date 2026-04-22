/*
 * max7219.h
 *
 * The driver exposes a flat 8x8 logical grid (x=0..7, y=0..7).
 * The physical 45-degree diamond shape comes purely from how the PCB
 * is mounted — the code treats it as a normal 8x8 matrix.
 *
 * inverted_matrix: set to MATRIX_A (0) or MATRIX_B (1) if that board
 * is physically soldered 180° rotated.  Set MAX7219_NO_INVERT otherwise.
 */
#ifndef MAX7219_H
#define MAX7219_H

#include "stm32f1xx_hal.h"

#define OP_NOOP        0x00
#define OP_DIGIT0      0x01
#define OP_DECODEMODE  0x09
#define OP_INTENSITY   0x0A
#define OP_SCANLIMIT   0x0B
#define OP_SHUTDOWN    0x0C
#define OP_DISPLAYTEST 0x0F

#define MAX7219_NO_INVERT 0xFF

typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef      *cs_port;
    uint16_t           cs_pin;
    uint8_t            num_devices;
    uint8_t            status[64];     /* shadow RAM: status[addr*8+row] */
    uint8_t            inverted_matrix;/* addr of 180°-rotated board, or 0xFF */
} Max7219_HandleTypeDef;

void    MAX7219_Init        (Max7219_HandleTypeDef *dev, SPI_HandleTypeDef *hspi,
                             GPIO_TypeDef *cs_port, uint16_t cs_pin, uint8_t num);
void    MAX7219_SetIntensity(Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t intensity);
void    MAX7219_Shutdown    (Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t shutdown);
void    MAX7219_ClearDisplay(Max7219_HandleTypeDef *dev, uint8_t addr);

void    MAX7219_SetXY       (Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t x, uint8_t y, uint8_t on);
uint8_t MAX7219_GetXY       (Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t x, uint8_t y);
void    MAX7219_InvertXY    (Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t x, uint8_t y);

void    MAX7219_Test_BlinkAll(Max7219_HandleTypeDef *dev);

void make_pattern(Max7219_HandleTypeDef *dev,uint8_t matrix_id);

#endif
