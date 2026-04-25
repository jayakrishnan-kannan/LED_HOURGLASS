/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include <stdlib.h>
#include "max7219.h"
#include "lis3dh.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
typedef struct {
	uint32_t start;
	uint32_t interval;
} NonBlockDelay_t;



/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
extern Max7219_HandleTypeDef  lc;
extern LIS3DH_HandleTypeDef   accel;
extern SPI_HandleTypeDef      hspi1;
extern I2C_HandleTypeDef      hi2c1;
extern uint8_t          delayHours, delayMinutes;
extern int              gravity;
extern bool             alarmWentOff;
extern NonBlockDelay_t  drop_delay;
extern uint16_t ADC_value;

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
#define CS_PORT             GPIOA
#define CS_PIN              GPIO_PIN_4
#define CS_PIN_Pin          GPIO_PIN_4
#define CS_PIN_GPIO_Port    GPIOA

//#define BUZZER_PORT         GPIOB
//#define BUZZER_PIN          GPIO_PIN_0

#define RESET_BTN_PORT      GPIOB
#define RESET_BTN_PIN       GPIO_PIN_1

#define LED_PORT            GPIOC
#define LED_PIN             GPIO_PIN_13  /* active LOW */

/* ── Matrix addresses ────────────────────────────────────────────── */
#define MATRIX_A   0u   /* top    rhombus */
#define MATRIX_B   1u   /* bottom rhombus */

/* ── Matrix Physical Rotations (adjust to match your PCB setup) ───── */
#define MATRIX_A_ROTATION  ROTATION_270      /* MATRIX_A rotation */
#define MATRIX_B_ROTATION  ROTATION_90    /* MATRIX_B rotation (180° from A) */

/* ── Physics constants ─────��─────────────��───────────────────────── */
#define SAND_GRAINS     60u   /* grains per chamber (≤ 64)   */
#define DELAY_FRAME_MS  80u   /* main-loop period ms          */
#define DEFAULT_HOURS   0u
#define DEFAULT_MINUTES 1u

/* ── Reset thresholds ────────────────────────────────────────────── */
#define RESET_HOLD_MS   1500u
#define SHAKE_THRESHOLD 2000
#define SHAKE_COUNT     2
/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
uint32_t millis(void);
void     hourglass_reset(void);
void     hourglass_flip(void);
uint8_t  hourglass_update(void);
uint8_t  hourglass_drop(void);
uint8_t  hourglass_count(uint8_t addr);
uint8_t  hourglass_top_matrix(void);
uint8_t  hourglass_settled(void);
uint8_t  hourglass_bottom_matrix(void);
void     display_led_count(uint8_t count);
void 	 clear_displays(void);
void     alarm_trigger(void);
void     Error_Handler(void);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define BUZZER_Pin GPIO_PIN_0
#define BUZZER_GPIO_Port GPIOA
#define CS_PIN_Pin GPIO_PIN_4
#define CS_PIN_GPIO_Port GPIOA
#define potentiometer_Pin GPIO_PIN_1
#define potentiometer_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
