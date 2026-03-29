#include "common_inc.h"
#include "adc.h"
#include "cmd_ctrl_motor.h"
#include "motor.h"
#include "mt6816_stm32.h"
#include "led_stm32.h"
#include "key_stm32.h"
#include "current_sense.h"
#include "voltage_sense.h"
#include "adspe_sense.h"
#include "ntc_sense.h"

/* Default Entry -------------------------------------------------------*/
CmdCtrlMotor* motor = new CmdCtrlMotor( 3, true, 30, 35, 180);
Motor focMotor = Motor(7);
MT6816 mt6816(&hspi1);
Led statusLed;
CurrentSense currentSense(0.001f, 20.0f);
VoltageSense voltageSense;
AdspeSense adspeSense;
NtcSense ntcSense;
Key key1(1,20,250,800);
Key key2(2,20,250,800);
Key key3(3,20,250,800);
Key key4(4,20,250,800);

osThreadId_t mt6816TaskHandle;
osThreadId_t peripheralTaskHandle;
osThreadId_t adcMonitorTaskHandle;

static void ReportTaskCreateFailure(const char* taskName)
{
    Respond(*usbStreamOutputPtr,
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

static long AbsLong(long v)
{
    return (v < 0) ? -v : v;
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
    Respond(*usbStreamOutputPtr, "[key] KEY%u %s", keyId, KeyEventToString(event));
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

    constexpr uint32_t kTickMs = 10U;
    constexpr uint32_t kLedTickMs = 50U;
    constexpr uint32_t kStateHoldMs = 5000U;
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
        key1.Tick(kTickMs);
        key2.Tick(kTickMs);
        key3.Tick(kTickMs);
        key4.Tick(kTickMs);

        elapsedForLed += kTickMs;
        if (elapsedForLed >= kLedTickMs)
        {
            elapsedForLed = 0;
            statusLed.Tick(kLedTickMs, currentState);
        }

        elapsedInState += kTickMs;
        if (elapsedInState >= kStateHoldMs)
        {
            elapsedInState = 0;
            currentState = NextSimState(currentState);
        }

        osDelay(kTickMs);
    }
}

static void ThreadAdcMonitor(void* argument)
{
    (void)argument;

    currentSense.Init();
    voltageSense.Init();
    adspeSense.Init();
    ntcSense.Init();

    for (;;)
    {
        const PhaseCurrent_t i = currentSense.GetPhaseCurrents();
        const PhaseVoltage_t v = voltageSense.GetPhaseVoltages();
        const float vBus = voltageSense.GetBusVoltage();
        const uint16_t adspeRaw = adspeSense.GetRaw();
        const float adspeVolt = adspeSense.GetVoltage();
        const uint16_t ntcRaw = ntcSense.GetRaw();
        const float ntcVolt = ntcSense.GetVoltage();
        const float ntcTemp = ntcSense.GetTemperatureC();

        const long ia_mA = (long)(i.a * 1000.0f);
        const long ib_mA = (long)(i.b * 1000.0f);
        const long ic_mA = (long)(i.c * 1000.0f);
        const long va_mV = (long)(v.a * 1000.0f);
        const long vb_mV = (long)(v.b * 1000.0f);
        const long vc_mV = (long)(v.c * 1000.0f);
        const long vbus_mV = (long)(vBus * 1000.0f);
        const long adspe_mV = (long)(adspeVolt * 1000.0f);
        const long ntc_mV = (long)(ntcVolt * 1000.0f);
        const long ntc_centi = (long)(ntcTemp * 100.0f);

        Respond(*usbStreamOutputPtr,
                "[adc] IA=%ld.%03ldA IB=%ld.%03ldA IC=%ld.%03ldA | VA=%ld.%03ldV VB=%ld.%03ldV VC=%ld.%03ldV VBUS=%ld.%03ldV | ADSPE=%u/%ld.%03ldV | NTC=%u/%ld.%03ldV/%ld.%02ldC",
                ia_mA / 1000, AbsLong(ia_mA % 1000),
                ib_mA / 1000, AbsLong(ib_mA % 1000),
                ic_mA / 1000, AbsLong(ic_mA % 1000),
                va_mV / 1000, AbsLong(va_mV % 1000),
                vb_mV / 1000, AbsLong(vb_mV % 1000),
                vc_mV / 1000, AbsLong(vc_mV % 1000),
                vbus_mV / 1000, AbsLong(vbus_mV % 1000),
                adspeRaw, adspe_mV / 1000, AbsLong(adspe_mV % 1000),
                ntcRaw, ntc_mV / 1000, AbsLong(ntc_mV % 1000),
                ntc_centi / 100, AbsLong(ntc_centi % 100));

        osDelay(500);
    }
}

void Main(void)
{
    // Init all communication staff, including USB-CDC/VCP/UART/CAN etc.
    InitCommunication();
    AdcStartDmaSampling();
    Respond(*usbStreamOutputPtr,
            "[sys] Heap cur=%u min=%u Bytes",
            (unsigned) xPortGetFreeHeapSize(),
            (unsigned) xPortGetMinimumEverFreeHeapSize());

    // const osThreadAttr_t mt6816Task_attributes = {
    //     .name = "mt6816Task",
    //     .stack_size = 1024,
    //     .priority = (osPriority_t)osPriorityBelowNormal,
    // };
    // mt6816TaskHandle = osThreadNew(ThreadMT6816, nullptr, &mt6816Task_attributes);
    // if (mt6816TaskHandle == nullptr)
    // {
    //     ReportTaskCreateFailure("mt6816Task");
    // }

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

    // const osThreadAttr_t adcMonitorTask_attributes = {
    //     .name = "adcMonitorTask",
    //     .stack_size = 2048,
    //     .priority = (osPriority_t)osPriorityLow,
    // };
    // adcMonitorTaskHandle = osThreadNew(ThreadAdcMonitor, nullptr, &adcMonitorTask_attributes);
    // if (adcMonitorTaskHandle == nullptr)
    // {
    //     ReportTaskCreateFailure("adcMonitorTask");
    // }
}
