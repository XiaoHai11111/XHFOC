#include "time_utils.h"

#include "stm32g4xx_hal.h"

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
    // Derive microseconds from HAL millisecond tick and SysTick reload counter.
    // This keeps timing monotonic enough for control-loop filters and PID.
    const uint32_t tms = SysTick->LOAD + 1U;

    (void)((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) != 0U);
    uint32_t ms = HAL_GetTick();
    uint32_t u = tms - SysTick->VAL;

    if ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) != 0U)
    {
        ms = HAL_GetTick();
        u = tms - SysTick->VAL;
    }

    return static_cast<uint64_t>(ms) * 1000ULL + (static_cast<uint64_t>(u) * 1000ULL) / static_cast<uint64_t>(tms);
}

uint32_t millis()
{
    return HAL_GetTick();
}
