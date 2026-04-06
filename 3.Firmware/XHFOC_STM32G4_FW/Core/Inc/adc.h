/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    adc.h
  * @brief   This file contains all the function prototypes for
  *          the adc.c file
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
#ifndef __ADC_H__
#define __ADC_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include <stdint.h>

/* USER CODE END Includes */

extern ADC_HandleTypeDef hadc1;

extern ADC_HandleTypeDef hadc2;

/* USER CODE BEGIN Private defines */
typedef enum
{
    ADC_SIGNAL_IA = 0,
    ADC_SIGNAL_IB,
    ADC_SIGNAL_IC,
    ADC_SIGNAL_NTC,
    ADC_SIGNAL_VA,
    ADC_SIGNAL_VB,
    ADC_SIGNAL_VC,
    ADC_SIGNAL_ADSPE,
    ADC_SIGNAL_VBUS
} AdcSignal_t;

/* USER CODE END Private defines */

void MX_ADC1_Init(void);
void MX_ADC2_Init(void);

/* USER CODE BEGIN Prototypes */
void AdcStartDmaSampling(void);
uint16_t AdcGetRaw(AdcSignal_t signal);
bool AdcGetInjectedPhaseCurrentsRaw(uint16_t* ia, uint16_t* ib, uint16_t* ic);
float AdcRawToVoltage(uint16_t raw);
float AdcGetVoltage(AdcSignal_t signal);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __ADC_H__ */

