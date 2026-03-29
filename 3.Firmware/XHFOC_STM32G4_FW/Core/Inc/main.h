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
#define LED_Red_Pin GPIO_PIN_13
#define LED_Red_GPIO_Port GPIOC
#define LED_Yellow_Pin GPIO_PIN_14
#define LED_Yellow_GPIO_Port GPIOC
#define LED_Green_Pin GPIO_PIN_15
#define LED_Green_GPIO_Port GPIOC
#define M0_VA_Pin GPIO_PIN_0
#define M0_VA_GPIO_Port GPIOC
#define M0_VB_Pin GPIO_PIN_1
#define M0_VB_GPIO_Port GPIOC
#define M0_VC_Pin GPIO_PIN_2
#define M0_VC_GPIO_Port GPIOC
#define M0_IC_Pin GPIO_PIN_0
#define M0_IC_GPIO_Port GPIOA
#define M0_IB_Pin GPIO_PIN_1
#define M0_IB_GPIO_Port GPIOA
#define M0_IA_Pin GPIO_PIN_2
#define M0_IA_GPIO_Port GPIOA
#define WS2812B_Pin GPIO_PIN_5
#define WS2812B_GPIO_Port GPIOA
#define M0_HALL_A_Pin GPIO_PIN_6
#define M0_HALL_A_GPIO_Port GPIOA
#define M0_HALL_B_Pin GPIO_PIN_7
#define M0_HALL_B_GPIO_Port GPIOA
#define ADSPE_Pin GPIO_PIN_4
#define ADSPE_GPIO_Port GPIOC
#define M0_VBUS_Pin GPIO_PIN_5
#define M0_VBUS_GPIO_Port GPIOC
#define M0_HALL_C_Pin GPIO_PIN_1
#define M0_HALL_C_GPIO_Port GPIOB
#define NTC_Pin GPIO_PIN_12
#define NTC_GPIO_Port GPIOB
#define M0_PWM_AL_Pin GPIO_PIN_13
#define M0_PWM_AL_GPIO_Port GPIOB
#define M0_PWM_BL_Pin GPIO_PIN_14
#define M0_PWM_BL_GPIO_Port GPIOB
#define M0_PWM_CL_Pin GPIO_PIN_15
#define M0_PWM_CL_GPIO_Port GPIOB
#define KEY4_Pin GPIO_PIN_6
#define KEY4_GPIO_Port GPIOC
#define KEY3_Pin GPIO_PIN_7
#define KEY3_GPIO_Port GPIOC
#define KEY2_Pin GPIO_PIN_8
#define KEY2_GPIO_Port GPIOC
#define KEY1_Pin GPIO_PIN_9
#define KEY1_GPIO_Port GPIOC
#define M0_PWM_AH_Pin GPIO_PIN_8
#define M0_PWM_AH_GPIO_Port GPIOA
#define M0_PWM_BH_Pin GPIO_PIN_9
#define M0_PWM_BH_GPIO_Port GPIOA
#define M0_PWM_CH_Pin GPIO_PIN_10
#define M0_PWM_CH_GPIO_Port GPIOA
#define SPI1_CSN_Pin GPIO_PIN_2
#define SPI1_CSN_GPIO_Port GPIOD

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
