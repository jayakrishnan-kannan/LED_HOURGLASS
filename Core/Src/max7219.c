/*
 * max7219.c  —  MAX7219 8×8 LED matrix driver, STM32 HAL SPI
 *
 * Per-matrix rotation:
 *   rotation_a  →  applied to addr 0  (MATRIX_A)
 *   rotation_b  →  applied to addr 1  (MATRIX_B)
 *
 * For the diamond hourglass:
 *   Matrix A top rhombus    → rotation_a = 0   (or whatever your wiring needs)
 *   Matrix B bottom rhombus → rotation_b = 180  (always 180° different from A)
 *
 * If display content appears wrong, increment the relevant rotation by 90
 * until it looks right.  Only two values in main.c need changing.
 */

#include "max7219.h"
#include <string.h>

/* ── SPI ─────────────────────────────────────────────────────────── */

static void spi_send(Max7219_HandleTypeDef *dev,
                     uint8_t addr, uint8_t opcode, uint8_t data)
{
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_RESET);
    for (int8_t i = (int8_t)(dev->num_devices - 1); i >= 0; i--) {
        uint8_t tx[2];
        if ((uint8_t)i == addr) { tx[0] = opcode; tx[1] = data; }
        else                    { tx[0] = OP_NOOP; tx[1] = 0;   }
        HAL_SPI_Transmit(dev->hspi, tx, 2, 100);
    }
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_SET);
}

static void flush_row(Max7219_HandleTypeDef *dev, uint8_t addr, uint8_t row)
{
    spi_send(dev, addr,
             (uint8_t)(OP_DIGIT0 + row),
             dev->status[addr * 8 + row]);
}

/* ── Coordinate transform ────────────────────────────────────────── */

/*
 * get_rotation — returns the hardware rotation for the given matrix address.
 * rotation_a applies to addr 0, rotation_b applies to addr 1.
 * For more than 2 devices extend this function.
 */
static int get_rotation(const Max7219_HandleTypeDef *dev, uint8_t addr)
{
    int rot = (addr == 0) ? dev->rotation_a : dev->rotation_b;
    return ((rot % 360) + 360) % 360;
}

/*
 * rotate_coord — apply a single rotation to (x,y).
 * This is the 0/90/180/270 standard 2D rotation on an 8×8 grid.
 */
static void rotate_coord(int rot, uint8_t *x, uint8_t *y)
{
    uint8_t tx = *x, ty = *y;
    switch (rot) {
        case 90:
            *x = ty;
            *y = 7 - tx;
            break;
        case 180:
            *x = 7 - tx;
            *y = 7 - ty;
            break;
        case 270:
            *x = 7 - ty;
            *y = tx;
            break;
        default:   /* 0° — no change */
            break;
    }
}

/*
 * apply_transform — full per-matrix transform for logical pixel access.
 * Step 1: apply the per-matrix hardware rotation.
 * Step 2: apply the inversion flag (for physically-reversed wiring).
 */
static void apply_transform(const Max7219_HandleTypeDef *dev,
                            uint8_t addr, uint8_t *x, uint8_t *y)
{
    rotate_coord(get_rotation(dev, addr), x, y);

    if (addr == dev->inverted_matrix) {
        *x = 7 - *x;
        *y = 7 - *y;
    }
}

/* apply_inversion — raw access: inversion only, no rotation */
static void apply_inversion(const Max7219_HandleTypeDef *dev,
                            uint8_t addr, uint8_t *x, uint8_t *y)
{
    if (addr == dev->inverted_matrix) {
        *x = 7 - *x;
        *y = 7 - *y;
    }
}

/* ── Shadow RAM bit helpers ──────────────────────────────────────── */

static void shadow_set(Max7219_HandleTypeDef *dev,
                       uint8_t addr, uint8_t x, uint8_t y, uint8_t on)
{
    uint8_t  bit  = (uint8_t)(0x80u >> x);   /* bit7 = col0, bit0 = col7 */
    uint8_t *slot = &dev->status[addr * 8 + y];
    if (on) *slot |=  bit;
    else    *slot &= (uint8_t)(~bit);
    flush_row(dev, addr, y);
}

static uint8_t shadow_get(const Max7219_HandleTypeDef *dev,
                          uint8_t addr, uint8_t x, uint8_t y)
{
    uint8_t bit = (uint8_t)(0x80u >> x);
    return (dev->status[addr * 8 + y] & bit) ? 1u : 0u;
}

/* ── Init & configuration ────────────────────────────────────────── */

void MAX7219_Init(Max7219_HandleTypeDef *dev,
                  SPI_HandleTypeDef     *hspi,
                  GPIO_TypeDef          *cs_port,
                  uint16_t               cs_pin,
                  uint8_t                num_devices)
{
    dev->hspi            = hspi;
    dev->cs_port         = cs_port;
    dev->cs_pin          = cs_pin;
    dev->num_devices     = (num_devices > 8u) ? 8u : num_devices;
    dev->rotation_a      = 0;      /* override in main.c after Init */
    dev->rotation_b      = 180;    /* override in main.c after Init */
    dev->inverted_matrix = MAX7219_NO_INVERT;
    memset(dev->status, 0, sizeof(dev->status));

    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);

    for (uint8_t i = 0; i < dev->num_devices; i++) {
        spi_send(dev, i, OP_DISPLAYTEST, 0x00);
        spi_send(dev, i, OP_SCANLIMIT,   0x07);
        spi_send(dev, i, OP_DECODEMODE,  0x00);
        MAX7219_ClearDisplay(dev, i);
        MAX7219_SetIntensity(dev, i, 1);
        MAX7219_Shutdown(dev, i, 0);
    }
}

void MAX7219_SetIntensity(Max7219_HandleTypeDef *dev,
                          uint8_t addr, uint8_t intensity)
{
    if (addr >= dev->num_devices) return;
    if (intensity > 15u) intensity = 15u;
    spi_send(dev, addr, OP_INTENSITY, intensity);
}

void MAX7219_Shutdown(Max7219_HandleTypeDef *dev,
                      uint8_t addr, uint8_t shutdown)
{
    if (addr >= dev->num_devices) return;
    spi_send(dev, addr, OP_SHUTDOWN, shutdown ? 0x00u : 0x01u);
}

void MAX7219_ClearDisplay(Max7219_HandleTypeDef *dev, uint8_t addr)
{
    if (addr >= dev->num_devices) return;
    for (uint8_t row = 0; row < 8; row++) {
        dev->status[addr * 8 + row] = 0;
        spi_send(dev, addr, (uint8_t)(OP_DIGIT0 + row), 0x00);
    }
}

/* ── Logical pixel access ────────────────────────────────────────── */

void MAX7219_SetXY(Max7219_HandleTypeDef *dev,
                   uint8_t addr, uint8_t x, uint8_t y, uint8_t on)
{
    if (addr >= dev->num_devices || x > 7u || y > 7u) return;
    apply_transform(dev, addr, &x, &y);
    shadow_set(dev, addr, x, y, on);
}

uint8_t MAX7219_GetXY(Max7219_HandleTypeDef *dev,
                      uint8_t addr, uint8_t x, uint8_t y)
{
    if (addr >= dev->num_devices || x > 7u || y > 7u) return 0;
    apply_transform(dev, addr, &x, &y);
    return shadow_get(dev, addr, x, y);
}

void MAX7219_InvertXY(Max7219_HandleTypeDef *dev,
                      uint8_t addr, uint8_t x, uint8_t y)
{
    MAX7219_SetXY(dev, addr, x, y, !MAX7219_GetXY(dev, addr, x, y));
}

/* ── Raw pixel access ────────────────────────────────────────────── */

void MAX7219_SetRawXY(Max7219_HandleTypeDef *dev,
                      uint8_t addr, uint8_t x, uint8_t y, uint8_t on)
{
    if (addr >= dev->num_devices || x > 7u || y > 7u) return;
    apply_inversion(dev, addr, &x, &y);
    shadow_set(dev, addr, x, y, on);
}

uint8_t MAX7219_GetRawXY(Max7219_HandleTypeDef *dev,
                         uint8_t addr, uint8_t x, uint8_t y)
{
    if (addr >= dev->num_devices || x > 7u || y > 7u) return 0;
    apply_inversion(dev, addr, &x, &y);
    return shadow_get(dev, addr, x, y);
}

void MAX7219_InvertRawXY(Max7219_HandleTypeDef *dev,
                         uint8_t addr, uint8_t x, uint8_t y)
{
    MAX7219_SetRawXY(dev, addr, x, y, !MAX7219_GetRawXY(dev, addr, x, y));
}

/* ── Diagnostics ─────────────────────────────────────────────────── */

void MAX7219_Test_BlinkAll(Max7219_HandleTypeDef *dev)
{
    for (uint8_t rep = 0; rep < 3; rep++) {
        for (uint8_t addr = 0; addr < dev->num_devices; addr++)
            for (uint8_t row = 0; row < 8; row++)
                spi_send(dev, addr, (uint8_t)(OP_DIGIT0 + row), 0xFF);
        HAL_Delay(300);
        for (uint8_t addr = 0; addr < dev->num_devices; addr++)
            for (uint8_t row = 0; row < 8; row++)
                spi_send(dev, addr, (uint8_t)(OP_DIGIT0 + row), 0x00);
        HAL_Delay(300);
    }

    /* Restore shadow RAM */
    for (uint8_t addr = 0; addr < dev->num_devices; addr++)
        for (uint8_t row = 0; row < 8; row++)
            flush_row(dev, addr, row);
}
