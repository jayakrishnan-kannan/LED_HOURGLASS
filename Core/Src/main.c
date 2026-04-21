/*
 * main.c  —  LED Diamond Hourglass, STM32F103C8 Blue Pill
 */

#include "main.h"

SPI_HandleTypeDef     hspi1;
I2C_HandleTypeDef     hi2c1;
Max7219_HandleTypeDef lc;
LIS3DH_HandleTypeDef  accel;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_I2C1_Init(void);

static void led_blink(uint32_t ms)
{
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
    HAL_Delay(ms);
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
    HAL_Delay(ms);
}

static void beep_confirm(void)
{
    for (int i = 0; i < 2; i++) {
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
        HAL_Delay(80);
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
        HAL_Delay(120);
    }
}

/* ── Reset trigger: push button (hold RESET_HOLD_MS) ────────────── */
static uint8_t check_reset_button(void)
{
    static uint32_t pressed_since = 0;
    static uint8_t  was_down      = 0;

    uint8_t down = (HAL_GPIO_ReadPin(RESET_BTN_PORT, RESET_BTN_PIN) == GPIO_PIN_RESET);

    if (down && !was_down)
        pressed_since = HAL_GetTick();

    if (down && (HAL_GetTick() - pressed_since >= RESET_HOLD_MS)) {
        was_down = 0;
        beep_confirm();
        hourglass_reset();
        return 1;
    }
    was_down = down;
    return 0;
}

/* ── Reset trigger: violent shake ───────────────────────────────── */
static uint8_t check_shake(void)
{
    static uint8_t shake_frames = 0;

    LIS3DH_ReadRaw(&accel);
    int32_t ax = accel.raw_x, ay = accel.raw_y, az = accel.raw_z;
    int32_t mag2    = ax*ax + ay*ay + az*az;
    int32_t thresh2 = (int32_t)SHAKE_THRESHOLD * (int32_t)SHAKE_THRESHOLD;

    if (mag2 > thresh2) {
        if (++shake_frames >= SHAKE_COUNT) {
            shake_frames = 0;
            hourglass_reset();
            return 1;
        }
    } else {
        shake_frames = 0;
    }
    return 0;
}

/* ================================================================== */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI1_Init();
    MX_I2C1_Init();

    /* ── MAX7219 ─────────────────────────────────────────────────── */
    MAX7219_Init(&lc, &hspi1, CS_PORT, CS_PIN, 2);

    /*
     * ══════════════════════════════════════════════════
     * ORIENTATION TUNING — change only ROT_A and ROT_B
     * in main.h.  The two should always differ by 180°.
     *
     *   lc.rotation_a = ROT_A   (Matrix A, top rhombus)
     *   lc.rotation_b = ROT_B   (Matrix B, bottom rhombus)
     *
     * If the top diamond looks upside-down:   swap ROT_A↔ROT_B
     * If it looks sideways:                   add 90 to both
     *
     * inverted_matrix: if one board's row/column wiring is physically
     * reversed (not just rotated), set this to MATRIX_A or MATRIX_B.
     * Leave as MAX7219_NO_INVERT if both boards look correct after
     * setting the rotation values.
     * ══════════════════════════════════════════════════
     */
    lc.rotation_a      = ROT_A;
    lc.rotation_b      = ROT_B;
    lc.inverted_matrix = MAX7219_NO_INVERT;

    MAX7219_SetIntensity(&lc, MATRIX_A, 1);
    MAX7219_SetIntensity(&lc, MATRIX_B, 1);

    srand(HAL_GetTick());

    /* ── LIS3DH ──────────────────────────────────────────────────── */
    while (LIS3DH_Init(&accel, &hi2c1) == 0)
        led_blink(200);

    gravity = LIS3DH_GetGravityDirection(&accel);

    MAX7219_Test_BlinkAll(&lc);
    hourglass_reset();

    /* ── Main loop ───────────────────────────────────────────────── */
    while (1) {
        HAL_Delay(DELAY_FRAME_MS);

        if (check_reset_button()) continue;
        if (check_shake())        continue;

        int new_grav = LIS3DH_GetGravityDirection(&accel);
        if (new_grav != gravity) {
            int delta = new_grav - gravity;
            if (delta < 0) delta = -delta;
            gravity = new_grav;

            if (delta == 180) {
                hourglass_flip();   /* preserve grain counts, restart timer */
                continue;
            }
            /* 90/270 sideways tilt: physics adapts via dy_direction() */
        }

        uint8_t moved   = hourglass_update();
        uint8_t dropped = hourglass_drop();

        if (!alarmWentOff && !moved && !dropped) {
            if (hourglass_count(hourglass_top_matrix()) == 0) {
                alarmWentOff = true;
                alarm_trigger();
            }
        }
        if (dropped) alarmWentOff = false;
    }
}

/* ── Clock: 72 MHz, HSE 8 MHz, PLL×9 ────────────────────────────── */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.HSIState       = RCC_HSI_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL     = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();
    clk.ClockType      = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                        |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
}

/* ── GPIO ────────────────────────────────────────────────────────── */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    HAL_GPIO_WritePin(CS_PORT,     CS_PIN,     GPIO_PIN_SET);
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_PORT,    LED_PIN,    GPIO_PIN_SET);

    g.Mode = GPIO_MODE_OUTPUT_PP; g.Pull = GPIO_NOPULL;
    g.Pin = CS_PIN;     g.Speed = GPIO_SPEED_FREQ_HIGH; HAL_GPIO_Init(CS_PORT, &g);
    g.Pin = BUZZER_PIN; g.Speed = GPIO_SPEED_FREQ_LOW;  HAL_GPIO_Init(BUZZER_PORT, &g);
    g.Pin = LED_PIN;    g.Speed = GPIO_SPEED_FREQ_LOW;  HAL_GPIO_Init(LED_PORT, &g);

    g.Pin  = RESET_BTN_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(RESET_BTN_PORT, &g);
}

/* ── SPI1: MAX7219, Mode 0, MSB, 8-bit ──────────────────────────── */
static void MX_SPI1_Init(void)
{
    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;
    hspi1.Init.NSS               = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial     = 10;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) Error_Handler();
}

/* ── I2C1: LIS3DH, 100 kHz ──────────────────────────────────────── */
static void MX_I2C1_Init(void)
{
    hi2c1.Instance             = I2C1;
    hi2c1.Init.ClockSpeed      = 100000;
    hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2     = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) Error_Handler();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) { HAL_GPIO_TogglePin(LED_PORT, LED_PIN); HAL_Delay(100); }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{ (void)file; (void)line; Error_Handler(); }
#endif
