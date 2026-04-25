/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : LED Diamond Hourglass — STM32F103C8 Blue Pill
  *
  * Buzzer: TIM3 CH1 PWM on PA6. No interrupt needed — hardware PWM generates
  * the tone. Call buzzer_on(freq_hz) / buzzer_off() from anywhere.
  *
  * Setting display: both matrices show LED count.
  * Bottom matrix (0..63 pixels available, we use 0..59 for 60 steps).
  * Each minute = 1 LED. Pot maps 0..4095 → 1..60 min.
  *
  * Tick display: one LED extinguishes every 30 sec during normal run,
  * giving a live "time remaining" indicator across both matrices.
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */
Max7219_HandleTypeDef lc;
LIS3DH_HandleTypeDef  accel;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ── Buzzer (TIM3 CH1 PWM, PA6) ─────────────────────────────────── */
/*
 * Blue Pill: TIM3 CH1 is on PA6 (no remap needed).
 * Wire buzzer between PA6 and GND (passive buzzer or transistor).
 * Active buzzer: just toggle PA6 as GPIO instead — but PWM gives
 * proper frequency control for passive buzzers.
 */
static void buzzer_on(uint32_t freq_hz)
{
    if (freq_hz == 0) return;
    /* TIM3 clock = PCLK1 * 2 = 72 MHz (with APB1 /2 prescaler) */
    uint32_t timer_clk = 72000000UL;
    uint32_t arr = (timer_clk / (htim2.Init.Prescaler + 1)) / freq_hz - 1;
    __HAL_TIM_SET_AUTORELOAD(&htim2, arr);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, arr / 2); /* 50% duty */
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
}

static void buzzer_off(void)
{
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
}

static void beep(uint32_t freq_hz, uint32_t ms)
{
    buzzer_on(freq_hz);
    HAL_Delay(ms);
    buzzer_off();
}

static void beep_confirm(void)
{
    beep(1000, 80); HAL_Delay(60);
    beep(1200, 80);
}

static void beep_tick(void)   /* one LED extinguished */
{
    beep(800, 20);
}

static void beep_alarm(void)  /* timer done */
{
    for (int i = 0; i < 5; i++) {
        beep(1500, 150); HAL_Delay(100);
        beep(900,  150); HAL_Delay(100);
    }
}

/* ── LED indicator helpers ───────────────────────────────────────── */
static void led_blink(uint32_t ms)
{
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
    HAL_Delay(ms);
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
    HAL_Delay(ms);
}

/* ── ADC / pot ───────────────────────────────────────────────────── */
static uint8_t read_pot_minutes(void)
{
    static uint32_t filtered = 0;
    static uint8_t  inited   = 0;

    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    uint32_t raw = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    if (!inited) { filtered = raw; inited = 1; }
    else         { filtered = (filtered * 3 + raw) / 4; }

    return (uint8_t)((filtered * 59) / 4095) + 1;  /* 1..60 */
}

/* display_led_count defined in functions.c */

/* ── Pot setting mode ────────────────────────────────────────────── */
#define POT_SETTLE_MS  2000   /* ms stable before confirming */
#define POT_DEADBAND   1

static void pot_setting_mode(void)
{
    /* Clear both matrices so old pixels don't confuse the new reading */
    MAX7219_ClearDisplay(&lc, MATRIX_A);
    MAX7219_ClearDisplay(&lc, MATRIX_B);

    uint8_t  last     = read_pot_minutes();
    uint32_t stable_t = HAL_GetTick();

    display_led_count(last * 2);  /* 2 LEDs per minute */

    while (1) {
        HAL_Delay(30);
        uint8_t cur = read_pot_minutes();
        uint8_t diff = (cur > last) ? (cur - last) : (last - cur);

        if (diff > POT_DEADBAND) {
            /* Pot moved — clear and redraw from scratch */
            MAX7219_ClearDisplay(&lc, MATRIX_A);
            MAX7219_ClearDisplay(&lc, MATRIX_B);
            last     = cur;
            stable_t = HAL_GetTick();
            display_led_count(cur * 2);  /* 2 LEDs per minute */
        } else if (HAL_GetTick() - stable_t >= POT_SETTLE_MS) {
            delayMinutes = last;
            delayHours   = 0;
            beep_confirm();
            //hourglass_reset();
            return;
        }
    }
}

/* ── Tick display (30 sec per LED) ──────────────────────────────── */
/*
 * Each call removes one LED from the display to show time passing.
 * Called from the main loop every TICK_INTERVAL_MS.
 * Works independently of sand physics — purely visual progress bar.
 */
#define TICK_INTERVAL_MS  30000UL   /* 30 seconds per LED */
/* TICK_LEDS_TOTAL computed at runtime: delayMinutes * 2 */

static uint8_t  tick_remaining  = 0;
static uint32_t tick_last_ms    = 0;
static uint8_t  tick_active     = 0;

static void tick_display_start(void)
{
    /* 1 LED = 30 sec → 2 LEDs per minute */
    tick_remaining = (uint8_t)(delayMinutes * 2);
    if (tick_remaining > 120) tick_remaining = 120;
    tick_last_ms   = HAL_GetTick();
    tick_active    = 1;
    display_led_count(tick_remaining);
}

static void tick_display_stop(void)
{
    tick_active = 0;
}

/*
 * Call once per main loop iteration.
 * Returns 1 if countdown finished (all LEDs gone).
 */
static uint8_t tick_display_update(void)
{
    if (!tick_active) return 0;
    if (tick_remaining == 0) return 1;

    if (HAL_GetTick() - tick_last_ms >= TICK_INTERVAL_MS) {
        tick_last_ms += TICK_INTERVAL_MS;
        tick_remaining--;
        display_led_count(tick_remaining);
        beep_tick();

        if (tick_remaining == 0) {
            tick_active = 0;
            return 1;
        }
    }
    return 0;
}

/* ── Reset button ────────────────────────────────────────────────── */
static uint8_t check_reset_button(void)
{
    static uint32_t pressed_since = 0;
    static uint8_t  was_down      = 0;

    uint8_t down = (HAL_GPIO_ReadPin(RESET_BTN_PORT, RESET_BTN_PIN) == GPIO_PIN_RESET);

    if (down && !was_down) pressed_since = HAL_GetTick();

    if (down && (HAL_GetTick() - pressed_since >= RESET_HOLD_MS)) {
        was_down = 0;
        beep_confirm();
        hourglass_reset();
        tick_display_start();
        return 1;
    }
    was_down = down;
    return 0;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */

    MAX7219_Init(&lc, &hspi1, CS_PORT, CS_PIN, 2);
    MAX7219_SetRotation(&lc, MATRIX_A, MATRIX_A_ROTATION);
    MAX7219_SetRotation(&lc, MATRIX_B, MATRIX_B_ROTATION);
    lc.inverted_matrix = MATRIX_A;

    MAX7219_SetIntensity(&lc, MATRIX_A, 1);
    MAX7219_SetIntensity(&lc, MATRIX_B, 1);

    srand(HAL_GetTick());

    while (LIS3DH_Init(&accel, &hi2c1) == 0)
        led_blink(200);

    gravity = LIS3DH_GetGravityDirection(&accel);

    MAX7219_Test_BlinkAll(&lc);
    delayMinutes = read_pot_minutes();
    delayHours   = 0;
    hourglass_reset();
    tick_display_start();

//    uint8_t pot_last = read_pot_minutes();
    uint8_t pot_last = delayMinutes;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	led_blink(80);
        HAL_Delay(DELAY_FRAME_MS);

//      if (check_reset_button()) continue;

        /* --- Pot movement detection → enter setting mode --- */
        uint8_t pot_now = read_pot_minutes();
        if ((pot_now > pot_last ? pot_now - pot_last
                                : pot_last - pot_now) > POT_DEADBAND) {
            tick_display_stop();
            pot_setting_mode();
            hourglass_reset();
            tick_display_start();
            pot_last = delayMinutes;
            continue;
        }
        pot_last = pot_now;

        /* --- Tick countdown display --- */
        if (tick_display_update()) {
            /* All LEDs gone — timer expired */
            beep_alarm();
            /* Restart automatically or wait for flip/reset */
        }

        /* --- Gravity / flip --- */
        int new_grav = LIS3DH_GetGravityDirection(&accel);
        if (new_grav != gravity) {
            int delta = new_grav - gravity;
            if (delta < 0) delta = -delta;
            gravity = new_grav;
            if (delta == 180) {
                hourglass_flip();
                tick_display_start();  /* restart countdown on flip */
                continue;
            }
        }

        /* --- Sand physics --- */
        hourglass_update();
        hourglass_drop();
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_9;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 50;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_ENABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_ENABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 71;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 65535;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CS_PIN_GPIO_Port, CS_PIN_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : CS_PIN_Pin */
  GPIO_InitStruct.Pin = CS_PIN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CS_PIN_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
