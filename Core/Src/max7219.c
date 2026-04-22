/*
 * max7219.c
 *
 * inverted_matrix: address of a board physically soldered 180° rotated.
 * The inversion is handled ONLY at the flush_row (SPI byte) level:
 *   - row r is sent as row (7-r)
 *   - the data byte is bit-reversed (left↔right mirror)
 * This means SetXY/GetXY logical coords are NEVER modified — the physics
 * engine, fill, and neck code all use correct logical coordinates.
 */
#include "max7219.h"
#include <string.h>

/* Reverse all 8 bits of a byte (mirror columns) */
static uint8_t byte_reverse(uint8_t b)
{
    b = (uint8_t)(((b & 0xF0)>>4) | ((b & 0x0F)<<4));
    b = (uint8_t)(((b & 0xCC)>>2) | ((b & 0x33)<<2));
    b = (uint8_t)(((b & 0xAA)>>1) | ((b & 0x55)<<1));
    return b;
}

static void spi_send(Max7219_HandleTypeDef *dev,
                     uint8_t addr, uint8_t opcode, uint8_t data)
{
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_RESET);
    for (int8_t i = (int8_t)(dev->num_devices-1); i >= 0; i--) {
        uint8_t tx[2];
        if ((uint8_t)i == addr) { tx[0]=opcode; tx[1]=data; }
        else                    { tx[0]=OP_NOOP; tx[1]=0;   }
        HAL_SPI_Transmit(dev->hspi, tx, 2, 100);
    }
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_SET);
}

/*
 * flush_row: write shadow RAM row to hardware.
 * For the inverted matrix: send row (7-row) with bit-reversed data byte.
 * This physically rotates the display 180° at the SPI level, keeping
 * all software coordinates unchanged.
 */
static void flush_row(Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t row)
{
    if (addr == dev->inverted_matrix) {
        uint8_t hw_row  = 7 - row;
        uint8_t hw_data = byte_reverse(dev->status[addr*8 + row]);
        spi_send(dev, addr, (uint8_t)(OP_DIGIT0 + hw_row), hw_data);
    } else {
        spi_send(dev, addr, (uint8_t)(OP_DIGIT0 + row), dev->status[addr*8+row]);
    }
}

/* Shadow RAM helpers — no coord transform here */
static void shadow_set(Max7219_HandleTypeDef *dev,
                       uint8_t addr, uint8_t x, uint8_t y, uint8_t on)
{
    uint8_t  bit  = (uint8_t)(0x80u >> x);
    uint8_t *slot = &dev->status[addr*8+y];
    if (on) *slot |=  bit;
    else    *slot &= (uint8_t)(~bit);
    flush_row(dev, addr, y);
}

static uint8_t shadow_get(const Max7219_HandleTypeDef *dev,
                           uint8_t addr, uint8_t x, uint8_t y)
{
    return (dev->status[addr*8+y] & (uint8_t)(0x80u>>x)) ? 1u : 0u;
}

/* Public API */
void MAX7219_Init(Max7219_HandleTypeDef *dev, SPI_HandleTypeDef *hspi,
                  GPIO_TypeDef *cs_port, uint16_t cs_pin, uint8_t num)
{
    dev->hspi = hspi; dev->cs_port = cs_port; dev->cs_pin = cs_pin;
    dev->num_devices = (num>8)?8:num;
    dev->inverted_matrix = MAX7219_NO_INVERT;
    memset(dev->status, 0, sizeof(dev->status));
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
    for (uint8_t i=0; i<dev->num_devices; i++) {
        spi_send(dev, i, OP_DISPLAYTEST, 0x00);
        spi_send(dev, i, OP_SCANLIMIT,   0x07);
        spi_send(dev, i, OP_DECODEMODE,  0x00);
        MAX7219_ClearDisplay(dev, i);
        MAX7219_SetIntensity(dev, i, 1);
        MAX7219_Shutdown(dev, i, 0);
    }
}

void MAX7219_SetIntensity(Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t v)
{ if (addr<dev->num_devices) spi_send(dev, addr, OP_INTENSITY, v>15?15:v); }

void MAX7219_Shutdown(Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t s)
{ if (addr<dev->num_devices) spi_send(dev, addr, OP_SHUTDOWN, s?0x00:0x01); }

void MAX7219_ClearDisplay(Max7219_HandleTypeDef *dev, uint8_t addr)
{
    if (addr>=dev->num_devices) return;
    for (uint8_t r=0; r<8; r++) {
        dev->status[addr*8+r] = 0;
        flush_row(dev, addr, r);
    }
}

void MAX7219_SetXY(Max7219_HandleTypeDef *dev,
                   uint8_t addr, uint8_t x, uint8_t y, uint8_t on)
{
    if (addr>=dev->num_devices || x>7 || y>7) return;
    shadow_set(dev, addr, x, y, on);
}

uint8_t MAX7219_GetXY(Max7219_HandleTypeDef *dev,
                      uint8_t addr, uint8_t x, uint8_t y)
{
    if (addr>=dev->num_devices || x>7 || y>7) return 0;
    return shadow_get(dev, addr, x, y);
}

void MAX7219_InvertXY(Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t x, uint8_t y)
{ MAX7219_SetXY(dev, addr, x, y, !MAX7219_GetXY(dev, addr, x, y)); }

void MAX7219_Test_BlinkAll(Max7219_HandleTypeDef *dev)
{
    for (uint8_t rep=0; rep<3; rep++) {
        for (uint8_t a=0; a<dev->num_devices; a++)
            for (uint8_t r=0; r<8; r++) {
                dev->status[a*8+r] = 0xFF;
                flush_row(dev, a, r);
            }
        HAL_Delay(300);
        for (uint8_t a=0; a<dev->num_devices; a++)
            for (uint8_t r=0; r<8; r++) {
                dev->status[a*8+r] = 0x00;
                flush_row(dev, a, r);
            }
        HAL_Delay(300);
    }
}

void make_pattern(Max7219_HandleTypeDef *dev,uint8_t matrix_id )
{
	for (uint8_t r=0; r<8; r++) {
		dev->status[1] = 0xFF;
		flush_row(dev, matrix_id, r);
	}
	HAL_Delay(300);
	for (uint8_t r=0; r<8; r++) {
		dev->status[matrix_id*8+r] = 0x00;
		flush_row(dev,matrix_id, r);
	}
	HAL_Delay(300);
}
