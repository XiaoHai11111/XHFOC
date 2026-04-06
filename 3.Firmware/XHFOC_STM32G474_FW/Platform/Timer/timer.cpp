#include "timer.hpp"

static TimerCallback_t timerCallbacks[5];

Timer::Timer(TIM_HandleTypeDef *_htim, uint32_t _freqHz)
{
    htim3.Instance = TIM3;

    if (!(_htim->Instance == TIM3))
    {
        Error_Handler();
    }

    if (_freqHz < 1) _freqHz = 1;
    else if (_freqHz > 10000000) _freqHz = 10000000;

    htim = _htim;
    freq = _freqHz;

    CalcRegister(freq);

    HAL_TIM_Base_DeInit(_htim);
    _htim->Init.Prescaler = PSC - 1;
    _htim->Init.CounterMode = TIM_COUNTERMODE_UP;
    _htim->Init.Period = ARR - 1;
    _htim->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    _htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(_htim) != HAL_OK)
    {
        Error_Handler();
    }
}

void Timer::Start()
{
    HAL_TIM_Base_Start_IT(htim);
}

void Timer::CalcRegister(uint32_t _freq)
{
    float psc = 0.5;
    float arr;

    do
    {
        psc *= 2;
        arr = 144000000.0f / psc / (float) _freq;
    } while (arr > 65535);

    if (htim->Instance == TIM3) // APB1 @84MHz
    {
        PSC = (uint16_t) round((double) psc);
        ARR = (uint16_t) (144000000.0f / (float) _freq / psc);
    }
}


void Timer::SetCallback(TimerCallback_t _timerCallback)
{
    if (htim->Instance == TIM3)
    {
        timerCallbacks[0] = _timerCallback;
    }
}


extern "C"
void OnTimerCallback(TIM_TypeDef *timInstance)
{
    if (timInstance == TIM3)
    {
        timerCallbacks[0]();
    }
}