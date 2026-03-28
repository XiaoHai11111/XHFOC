#include "common_inc.h"
#include "cmd_ctrl_motor.h"
#include "motor.h"
#include "mt6816_stm32.h"
#include "led_stm32.h"

/* Default Entry -------------------------------------------------------*/
CmdCtrlMotor* motor = new CmdCtrlMotor( 3, true, 30, 35, 180);
Motor focMotor = Motor(7);
MT6816 mt6816(&hspi1);
Led statusLed;

osThreadId_t mt6816TaskHandle;
osThreadId_t peripheralTaskHandle;


static Motor::RunState_t NextSimState(Motor::RunState_t current)
{
    switch (current)
    {
        case Motor::STATE_STOP: return Motor::STATE_RUNNING;
        case Motor::STATE_RUNNING: return Motor::STATE_FINISH;
        case Motor::STATE_FINISH: return Motor::STATE_NO_CALIB;
        case Motor::STATE_NO_CALIB: return Motor::STATE_STALL;
        case Motor::STATE_STALL: return Motor::STATE_OVERLOAD;
        case Motor::STATE_OVERLOAD: return Motor::STATE_STOP;
        default: return Motor::STATE_STOP;
    }
}

static void ThreadMT6816(void* argument)
{
    (void)argument;

    osDelay(100);
    bool initOk = mt6816.Init();
    Respond(*usbStreamOutputPtr, "[mt6816] init cal=%d", initOk ? 1 : 0);

    uint32_t printDivider = 0;
    for (;;)
    {
        uint16_t rectifiedAngle = mt6816.UpdateAngle();
        if ((printDivider++ % 20U) == 0U)
        {
            Respond(*usbStreamOutputPtr,
                    "[mt6816] raw=%u rect=%u chk=%d nomag=%d",
                    mt6816.angleData.rawAngle,
                    rectifiedAngle,
                    mt6816.IsChecksumValid() ? 1 : 0,
                    mt6816.IsNoMagnetDetected() ? 1 : 0);
        }
        osDelay(100);
    }
}

static void ThreadPeripheral(void* argument)
{
    (void)argument;

    constexpr uint32_t kTickMs = 50U;
    constexpr uint32_t kStateHoldMs = 5000U;
    uint32_t elapsedInState = 0;
    Motor::RunState_t currentState = Motor::STATE_STOP;

    for (;;)
    {
        statusLed.Tick(kTickMs, currentState);

        elapsedInState += kTickMs;
        if (elapsedInState >= kStateHoldMs)
        {
            elapsedInState = 0;
            currentState = NextSimState(currentState);
        }

        osDelay(kTickMs);
    }
}

void Main(void)
{
    // Init all communication staff, including USB-CDC/VCP/UART/CAN etc.
    InitCommunication();
    Respond(*usbStreamOutputPtr, "[sys] Heap remain: %d Bytes", xPortGetMinimumEverFreeHeapSize());

    const osThreadAttr_t mt6816Task_attributes = {
        .name = "mt6816Task",
        .stack_size = 1024,
        .priority = (osPriority_t)osPriorityBelowNormal,
    };
    mt6816TaskHandle = osThreadNew(ThreadMT6816, nullptr, &mt6816Task_attributes);

    const osThreadAttr_t peripheralTask_attributes = {
        .name = "peripheralTask",
        .stack_size = 1024,
        .priority = (osPriority_t)osPriorityLow,
    };
    peripheralTaskHandle = osThreadNew(ThreadPeripheral, nullptr, &peripheralTask_attributes);
}
