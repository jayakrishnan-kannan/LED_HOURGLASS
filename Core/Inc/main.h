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

#include<stdlib.h>
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
extern uint8_t delayHours;
extern uint8_t delayMinutes;
extern int gravity;
extern bool alarmWentOff;
extern NonBlockDelay_t drop_delay;
extern Max7219_HandleTypeDef lc;
extern LIS3DH_HandleTypeDef accel;
extern SPI_HandleTypeDef hspi1;
extern I2C_HandleTypeDef hi2c1;

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
#define CS_GPIO_PORT    GPIOA
#define CS_PIN          GPIO_PIN_4
#define BUZZER_PORT     GPIOB
#define BUZZER_PIN      GPIO_PIN_0

// Original defines
#define MATRIX_A 1
#define MATRIX_B 0
#define ACC_THRESHOLD_LOW  300
#define ACC_THRESHOLD_HIGH 360
#define DELAY_FRAME        100
/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define CS_PIN_Pin GPIO_PIN_4
#define CS_PIN_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */
uint32_t millis(void);
void resetTime(void);
uint8_t updateMatrix();
uint8_t dropParticle(void);
uint8_t countParticles(uint8_t addr);
uint8_t getTopMatrix(void);
void alarm();
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
