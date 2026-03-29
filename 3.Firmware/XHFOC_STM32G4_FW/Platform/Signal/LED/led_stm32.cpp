#include "led_stm32.h"
#include <gpio.h>

void Led::SetLedState(uint8_t _id, bool _state)
{
    switch (_id)
    {
        case 0:
            HAL_GPIO_WritePin(LED_Red_GPIO_Port, LED_Red_Pin, _state ? GPIO_PIN_SET : GPIO_PIN_RESET);
            break;
        case 1:
            HAL_GPIO_WritePin(LED_Green_GPIO_Port, LED_Green_Pin, _state ? GPIO_PIN_SET : GPIO_PIN_RESET);
            break;
        case 2:
            HAL_GPIO_WritePin(LED_Yellow_GPIO_Port, LED_Yellow_Pin, _state ? GPIO_PIN_SET : GPIO_PIN_RESET);
            break;
        default:
            break;
    }
}
