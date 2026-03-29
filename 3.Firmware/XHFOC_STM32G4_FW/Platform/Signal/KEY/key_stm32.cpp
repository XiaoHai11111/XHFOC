#include "key_stm32.h"
#include "main.h"

bool Key::ReadButtonPressed(uint8_t id)
{
    GPIO_TypeDef* port = KEY1_GPIO_Port;
    uint16_t pin = KEY1_Pin;

    switch (id)
    {
        case 1:
            port = KEY1_GPIO_Port;
            pin = KEY1_Pin;
            break;
        case 2:
            port = KEY2_GPIO_Port;
            pin = KEY2_Pin;
            break;
        case 3:
            port = KEY3_GPIO_Port;
            pin = KEY3_Pin;
            break;
        case 4:
            port = KEY4_GPIO_Port;
            pin = KEY4_Pin;
            break;
        default:
            return false;
    }

    return HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET;
}
