#include "common_inc.h"
#include "st_hardware.h"
/* Component Definitions -----------------------------------------------------*/

/* Main Entry ----------------------------------------------------------------*/
void Main()
{
    uint64_t serialNum = GetSerialNumber();
    printf("SerialNum:%d\r\n",(int)serialNum);

    /*------- Start Close-Loop Control Tick ------*/
    //HAL_Delay(100);
    //HAL_TIM_Base_Start_IT(&htim5);
}
/* Event Callbacks -----------------------------------------------------------*/
// extern "C" void Tim5Callback10s()
// {
//     __HAL_TIM_CLEAR_IT(&htim5, TIM_IT_UPDATE);
//     printf("Temp:%f\r\n", AdcGetChipTemperature());
// }
