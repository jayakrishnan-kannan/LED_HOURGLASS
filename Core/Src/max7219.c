/*
 * max7219.c
 *
 * Per-matrix rotation support for independently rotated LED matrices.
 * Logical coordinates (used by physics engine) are transformed to physical
 * coordinates based on each matrix's rotation setting.
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

/* Transform logical (x,y) to physical (px,py) based on rotation */
static void rotate_coord(uint8_t x, uint8_t y, MatrixRotation_t rot,
                         uint8_t *px, uint8_t *py)
{
    switch (rot) {
        case ROTATION_0:
            *px = x;
            *py = y;
            break;
        case ROTATION_90:  /* 90° clockwise */
            *px = (uint8_t)(7 - y);
            *py = x;
            break;
        case ROTATION_180:  /* 180° */
            *px = (uint8_t)(7 - x);
            *py = (uint8_t)(7 - y);
            break;
        case ROTATION_270:  /* 270° clockwise (90° counter-clockwise) */
            *px = y;
            *py = (uint8_t)(7 - x);
            break;
    }
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
 * Applies both legacy inversion (for 180° rotations) and new rotation system.
 */
static void flush_row(Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t row)
{
    uint8_t hw_row = row;
    uint8_t hw_data = dev->status[addr*8 + row];

    /* Apply legacy inversion if set (backward compatibility) */
    if (dev->inverted_matrix == addr) {
        hw_row  = 7 - row;
        hw_data = byte_reverse(hw_data);
    }

    spi_send(dev, addr, (uint8_t)(OP_DIGIT0 + hw_row), hw_data);
}

/* Shadow RAM helpers — no coord transform here */
static void shadow_set(Max7219_HandleTypeDef *dev,
                       uint8_t addr, uint8_t x, uint8_t y, uint8_t on)
{
    /* Transform logical (x,y) to physical (px,py) */
    uint8_t px, py;
    rotate_coord(x, y, dev->rotation[addr], &px, &py);

    uint8_t  bit  = (uint8_t)(0x80u >> px);
    uint8_t *slot = &dev->status[addr*8 + py];
    if (on) *slot |=  bit;
    else    *slot &= (uint8_t)(~bit);
    flush_row(dev, addr, py);
}

static uint8_t shadow_get(const Max7219_HandleTypeDef *dev,
                           uint8_t addr, uint8_t x, uint8_t y)
{
    /* Transform logical (x,y) to physical (px,py) */
    uint8_t px, py;

    /* Need non-const version, so cast away const for this read-only operation */
    rotate_coord(x, y, dev->rotation[addr], &px, &py);

    return (dev->status[addr*8 + py] & (uint8_t)(0x80u >> px)) ? 1u : 0u;
}

/* Public API */
void MAX7219_Init(Max7219_HandleTypeDef *dev, SPI_HandleTypeDef *hspi,
                  GPIO_TypeDef *cs_port, uint16_t cs_pin, uint8_t num)
{
    dev->hspi = hspi; dev->cs_port = cs_port; dev->cs_pin = cs_pin;
    dev->num_devices = (num>8)?8:num;
    dev->inverted_matrix = MAX7219_NO_INVERT;

    /* Initialize all matrices to 0° rotation (no rotation) */
    for (uint8_t i = 0; i < 2; i++) {
        dev->rotation[i] = ROTATION_0;
    }

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

void MAX7219_SetRotation(Max7219_HandleTypeDef *dev, uint8_t addr, MatrixRotation_t rot)
{
    if (addr < dev->num_devices) {
        dev->rotation[addr] = rot;
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
		dev->status[matrix_id*8+r] = 0xFF;
		flush_row(dev, matrix_id, r);
	}
	HAL_Delay(300);
	for (uint8_t r=0; r<8; r++) {
		dev->status[matrix_id*8+r] = 0x00;
		flush_row(dev,matrix_id, r);
	}
	HAL_Delay(300);
}
