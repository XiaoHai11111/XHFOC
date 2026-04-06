#include "driver.h"
#include "tim.h"


inline float Constraint(float _val)
{
    if (_val > 1) _val = 1;
    else if (_val < 0) _val = 0;

    return _val;
}


bool Driver::ConfigTimerForPwm()
{
    if ((htim1.Instance != TIM1) || (htim1.State == HAL_TIM_STATE_RESET))
    {
        MX_TIM1_Init();
    }

    bool ok = true;
    // Match reference project timing: trigger ADC injected conversion near PWM period end.
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, (uint16_t)(PERIOD_COUNT - 10));
    ok = ok && (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) == HAL_OK);
    ok = ok && (HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1) == HAL_OK);
    ok = ok && (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2) == HAL_OK);
    ok = ok && (HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2) == HAL_OK);
    ok = ok && (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3) == HAL_OK);
    ok = ok && (HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_3) == HAL_OK);
    ok = ok && (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4) == HAL_OK);
    if (!ok)
    {
        (void)HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
        (void)HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_1);
        (void)HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_2);
        (void)HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_2);
        (void)HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_3);
        (void)HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_3);
        (void)HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_4);
        return false;
    }

    SetPwmDutyByRegister(0, 0, 0);


    return true;
}


void Driver::SetPwmDutyByRegister(float _dutyA, float _dutyB, float _dutyC)
{
    _dutyA = Constraint(_dutyA);
    _dutyB = Constraint(_dutyB);
    _dutyC = Constraint(_dutyC);

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, (uint16_t) (_dutyA * PERIOD_COUNT));
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, (uint16_t) (_dutyB * PERIOD_COUNT));
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, (uint16_t) (_dutyC * PERIOD_COUNT));
}
