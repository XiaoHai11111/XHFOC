#include "common_inc.h"
#include "adc.h"
#include "tim.h"
#include "cmd_ctrl_motor.h"
#include "motor.h"
#include "mt6816_stm32.h"
#include "driver.h"
#include "led_stm32.h"
#include "key_stm32.h"
#include "current_sense.h"
#include "timer.hpp"
#include <cstring>

/* Default Entry -------------------------------------------------------*/
CmdCtrlMotor* motor = new CmdCtrlMotor( 3, true, 30, 35, 180);
Motor focMotor = Motor(7);
MT6816 mt6816(&hspi1);
Timer timerCtrlLoop(&htim3, 5000);
Driver focDriver(12.0f);
Led statusLed;
CurrentSense currentSense(0.001f, 20.0f);
Key key1(1,20,250,800);
Key key2(2,20,250,800);
Key key3(3,20,250,800);
Key key4(4,20,250,800);

osThreadId_t focControlTaskHandle;
osThreadId_t peripheralTaskHandle;

struct FocDebugDutySample_t
{
    volatile uint32_t seq;
    volatile float a;
    volatile float b;
    volatile float c;
};

static FocDebugDutySample_t gFocDebugDutySample = {};

extern "C" void OnFocPwmDutyUpdateFromControlLoop(float dutyA, float dutyB, float dutyC)
{
    gFocDebugDutySample.seq++;
    gFocDebugDutySample.a = dutyA;
    gFocDebugDutySample.b = dutyB;
    gFocDebugDutySample.c = dutyC;
    gFocDebugDutySample.seq++;
}

static bool LoadFocDebugDutySample(float& dutyA, float& dutyB, float& dutyC)
{
    uint32_t seq0 = 0;
    uint32_t seq1 = 0;
    do
    {
        seq0 = gFocDebugDutySample.seq;
        dutyA = gFocDebugDutySample.a;
        dutyB = gFocDebugDutySample.b;
        dutyC = gFocDebugDutySample.c;
        seq1 = gFocDebugDutySample.seq;
    } while ((seq0 != seq1) || ((seq0 & 1U) != 0U));

    return (seq0 != 0U);
}

static void SendFocDebugFrameJustFloat500Hz()
{
    float dutyA = 0.0f;
    float dutyB = 0.0f;
    float dutyC = 0.0f;
    if (!LoadFocDebugDutySample(dutyA, dutyB, dutyC))
    {
        return;
    }
    if (uart3StreamOutputPtr == nullptr)
    {
        return;
    }

    constexpr uint8_t kTail[4] = {0x00U, 0x00U, 0x80U, 0x7FU};
    float ch[3] = {dutyA, dutyB, dutyC};
    uint8_t frame[sizeof(ch) + sizeof(kTail)] = {};
    memcpy(frame, ch, sizeof(ch));
    memcpy(frame + sizeof(ch), kTail, sizeof(kTail));
    (void)uart3StreamOutputPtr->process_bytes(frame, sizeof(frame), nullptr);
}

static void ReportTaskCreateFailure(const char* taskName)
{
    Respond(*uart3StreamOutputPtr,
            "[err] create task %s failed, heap cur=%u min=%u",
            taskName,
            (unsigned) xPortGetFreeHeapSize(),
            (unsigned) xPortGetMinimumEverFreeHeapSize());
}


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

static const char* KeyEventToString(KeyBase::Event event)
{
    switch (event)
    {
        case KeyBase::EVENT_SINGLE_CLICK: return "single";
        case KeyBase::EVENT_DOUBLE_CLICK: return "double";
        case KeyBase::EVENT_LONG_PRESS: return "long";
        default: return "unknown";
    }
}

static void OnKeyEvent(uint8_t keyId, KeyBase::Event event)
{
    Respond(*uart3StreamOutputPtr, "[key] KEY%u %s", keyId, KeyEventToString(event));
}

static void ThreadPeripheral(void* argument)
{
    (void)argument;

    constexpr uint32_t kLoopMs = 2U;        // 500 Hz debug output pace
    constexpr uint32_t kTickMs = 10U;       // key scan period
    constexpr uint32_t kLedTickMs = 50U;
    constexpr uint32_t kStateHoldMs = 5000U;
    uint32_t elapsedForKeys = 0;
    uint32_t elapsedInState = 0;
    uint32_t elapsedForLed = 0;
    Motor::RunState_t currentState = Motor::STATE_STOP;

    key1.SetOnEventListener(OnKeyEvent);
    key2.SetOnEventListener(OnKeyEvent);
    key3.SetOnEventListener(OnKeyEvent);
    key4.SetOnEventListener(OnKeyEvent);

    key1.Init();
    key2.Init();
    key3.Init();
    key4.Init();

    for (;;)
    {
        SendFocDebugFrameJustFloat500Hz();

        elapsedForKeys += kLoopMs;
        if (elapsedForKeys >= kTickMs)
        {
            elapsedForKeys -= kTickMs;
            key1.Tick(kTickMs);
            key2.Tick(kTickMs);
            key3.Tick(kTickMs);
            key4.Tick(kTickMs);
        }

        elapsedForLed += kLoopMs;
        if (elapsedForLed >= kLedTickMs)
        {
            elapsedForLed = 0;
            statusLed.Tick(kLedTickMs, currentState);
        }

        elapsedInState += kLoopMs;
        if (elapsedInState >= kStateHoldMs)
        {
            elapsedInState = 0;
            currentState = NextSimState(currentState);
        }

        osDelay(kLoopMs);
    }
}

static void ThreadFocControl(void* argument)
{
    (void)argument;

    constexpr uint32_t kControlLoopHz = 5000U;
    constexpr float kOpenLoopDirection = 1.0f;   // +1 or -1
    constexpr float kOpenLoopTargetFixed = 56.52f; // mechanical rad/s

    focMotor.SetControlLoopHz((float)kControlLoopHz);
    focMotor.AttachDriver(&focDriver);
    focMotor.AttachCurrentSense(&currentSense);
    focMotor.config.controlMode = Motor::VELOCITY_OPEN_LOOP;
    focMotor.config.voltageLimit = 0.8f;
    focMotor.config.currentLimit = 0.6f;
    focMotor.config.velocityLimit = kOpenLoopTargetFixed;

    const bool focInitOk = focMotor.Init();
    Respond(*uart3StreamOutputPtr, "[foc] init=%d mode=%d calib=%d err=%d dir=%.0f",
            focInitOk ? 1 : 0,
            static_cast<int>(focMotor.config.controlMode),
            mt6816.IsCalibrated() ? 1 : 0,
            static_cast<int>(focMotor.error),
            (double)kOpenLoopDirection);

    if (focInitOk)
    {
        focMotor.target = kOpenLoopTargetFixed * kOpenLoopDirection;
        focMotor.SetEnable(true);
        Respond(*uart3StreamOutputPtr, "[foc] enabled target=%.3f", (double)focMotor.target);
    }
    else
    {
        focMotor.SetEnable(false);
        Respond(*uart3StreamOutputPtr,
                "[foc] init failed, tim1_state=%d arr=%lu",
                (int)htim1.State,
                (unsigned long)__HAL_TIM_GET_AUTORELOAD(&htim1));
    }

    for (;;)
    {
        // Control loop is triggered by TIM3 update interrupt at 5 kHz.
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        focMotor.Tick();
    }
}

/* Timer Callbacks -------------------------------------------------------*/
void OnTimerCallback()
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Wake & invoke thread IMMEDIATELY.
    vTaskNotifyGiveFromISR(TaskHandle_t(focControlTaskHandle), &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


void Main(void)
{
    // Init all communication staff, including USB-CDC/VCP/UART/CAN etc.
    InitCommunication();
    AdcStartDmaSampling();
    Respond(*uart3StreamOutputPtr,
            "[sys] Heap cur=%u min=%u Bytes",
            (unsigned) xPortGetFreeHeapSize(),
            (unsigned) xPortGetMinimumEverFreeHeapSize());


    const osThreadAttr_t focControlTask_attributes = {
        .name = "focControlTask",
        .stack_size = 2048,
        .priority = (osPriority_t)osPriorityRealtime,
    };
    focControlTaskHandle = osThreadNew(ThreadFocControl, nullptr, &focControlTask_attributes);
    if (focControlTaskHandle == nullptr)
    {
        ReportTaskCreateFailure("focControlTask");
    }
    else
    {
        timerCtrlLoop.SetCallback(OnTimerCallback);
        timerCtrlLoop.Start();
    }

    const osThreadAttr_t peripheralTask_attributes = {
        .name = "peripheralTask",
        .stack_size = 1024,
        .priority = (osPriority_t)osPriorityLow,
    };
    peripheralTaskHandle = osThreadNew(ThreadPeripheral, nullptr, &peripheralTask_attributes);
    if (peripheralTaskHandle == nullptr)
    {
        ReportTaskCreateFailure("peripheralTask");
    }

}
