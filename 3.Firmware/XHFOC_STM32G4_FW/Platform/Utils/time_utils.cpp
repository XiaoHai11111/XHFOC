#include "time_utils.h"

#include "stm32g4xx_hal.h"
#include "tim.h"

static volatile uint32_t g_tim7OverflowCount = 0;

extern "C" void TimeUtilsOnTim7PeriodElapsedFromISR(void)
{
    g_tim7OverflowCount++;
}

void delay(uint32_t ms)
{
    const uint64_t t0 = micros();
    while ((micros() - t0) < (static_cast<uint64_t>(ms) * 1000ULL))
    {
        __NOP();
    }
}

void delayMicroSeconds(uint32_t us)
{
    const uint64_t t0 = micros();
    while ((micros() - t0) < static_cast<uint64_t>(us))
    {
        __NOP();
    }
}

uint64_t micros()
{
    if (htim7.Instance == nullptr)
    {
        return static_cast<uint64_t>(HAL_GetTick()) * 1000ULL;
    }

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    uint32_t overflow = g_tim7OverflowCount;
    uint32_t uif_before = (__HAL_TIM_GET_FLAG(&htim7, TIM_FLAG_UPDATE) != RESET) ? 1U : 0U;
    uint32_t counter = __HAL_TIM_GET_COUNTER(&htim7) & 0xFFFFU;
    uint32_t uif_after = (__HAL_TIM_GET_FLAG(&htim7, TIM_FLAG_UPDATE) != RESET) ? 1U : 0U;

    if (!primask)
    {
        __enable_irq();
    }

    if (uif_before)
    {
        // 溢出发生在读 counter 之前，counter 已经是新周期的值
        overflow += 1U;
    }
    else if (uif_after)
    {
        // 溢出发生在读 counter 之后，counter 是旧周期值，需要重读
        overflow += 1U;

        primask = __get_PRIMASK();
        __disable_irq();
        counter = __HAL_TIM_GET_COUNTER(&htim7) & 0xFFFFU;
        if (!primask)
        {
            __enable_irq();
        }
    }

    return (static_cast<uint64_t>(overflow) << 16U) | static_cast<uint64_t>(counter);
}

uint32_t millis()
{
    return HAL_GetTick();
}
