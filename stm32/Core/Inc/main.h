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
#include "stm32g4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define CH16_Pin GPIO_PIN_0
#define CH16_GPIO_Port GPIOC
#define CH15_Pin GPIO_PIN_1
#define CH15_GPIO_Port GPIOC
#define CH14_Pin GPIO_PIN_2
#define CH14_GPIO_Port GPIOC
#define CH13_Pin GPIO_PIN_3
#define CH13_GPIO_Port GPIOC
#define CH12_Pin GPIO_PIN_0
#define CH12_GPIO_Port GPIOA
#define CH11_Pin GPIO_PIN_1
#define CH11_GPIO_Port GPIOA
#define CH10_Pin GPIO_PIN_2
#define CH10_GPIO_Port GPIOA
#define CH9_Pin GPIO_PIN_3
#define CH9_GPIO_Port GPIOA
#define CH1_Pin GPIO_PIN_6
#define CH1_GPIO_Port GPIOC
#define CH2_Pin GPIO_PIN_7
#define CH2_GPIO_Port GPIOC
#define CH3_Pin GPIO_PIN_8
#define CH3_GPIO_Port GPIOC
#define CH4_Pin GPIO_PIN_9
#define CH4_GPIO_Port GPIOC
#define DIR_Pin GPIO_PIN_15
#define DIR_GPIO_Port GPIOA
#define TX_Pin GPIO_PIN_10
#define TX_GPIO_Port GPIOC
#define RX_Pin GPIO_PIN_11
#define RX_GPIO_Port GPIOC
#define CH5_Pin GPIO_PIN_6
#define CH5_GPIO_Port GPIOB
#define CH6_Pin GPIO_PIN_7
#define CH6_GPIO_Port GPIOB
#define CH7_Pin GPIO_PIN_8
#define CH7_GPIO_Port GPIOB
#define CH8_Pin GPIO_PIN_9
#define CH8_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
