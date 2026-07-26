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
#include "adspe_sense.h"
#include "vofa_debug.h"
#include "encoder_calibration_storage.h"
#include <cstring>

/* Default Entry -------------------------------------------------------*/
constexpr uint32_t kMotorPolePairs = 7U;
Motor focMotor = Motor(static_cast<int>(kMotorPolePairs));
CmdCtrlMotor* motor = new CmdCtrlMotor(focMotor, 3, true, 30, 35, 180);
MT6816 mt6816(&hspi1);
Driver focDriver(12.0f);
Led statusLed;
CurrentSense currentSense(0.001f, 62.0f);
AdspeSense adspeSense;
Key key1(1,20,250,800);
Key key2(2,20,250,800);
Key key3(3,20,250,800);
Key key4(4,20,250,800);



osThreadId_t focControlTaskHandle;
osThreadId_t peripheralTaskHandle;
osThreadId_t vofaTaskHandle;

static volatile bool gFocTickEnabled = false;
VofaDebug vofaDebug;

extern "C" void OnFocTimerElapsedFromISR(void)
{
    if (focControlTaskHandle == nullptr)
    {
        return;
    }

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(TaskHandle_t(focControlTaskHandle), &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
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

static const char* CalibrationLoadResultToString(EncoderCalibrationStorage::LoadResult result)
{
    switch (result)
    {
        case EncoderCalibrationStorage::LoadResult::VALID: return "valid";
        case EncoderCalibrationStorage::LoadResult::EMPTY: return "empty";
        case EncoderCalibrationStorage::LoadResult::INVALID: return "invalid";
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

    focMotor.AttachDriver(&focDriver);
    focMotor.AttachEncoder(&mt6816);
    focMotor.AttachCurrentSense(&currentSense);
    if (currentSense.gainA > 0.0f) currentSense.gainA = -currentSense.gainA;
    if (currentSense.gainB > 0.0f) currentSense.gainB = -currentSense.gainB;
    if (currentSense.gainC > 0.0f) currentSense.gainC = -currentSense.gainC;

    focMotor.config.controlMode = Motor::ANGLE;
    focMotor.target = 3.140f;

    // Current loop runs once per PWM period (TIM1 10 kHz center-aligned),
    // so dt = 100 us; velocity loop is auto-decimated to 1 kHz internally.
    focMotor.SetControlLoopHz(10000.0f);

    EncoderCalibrationStorage encoderCalibrationStorage;
    EncoderCalibrationStorage::Data storedCalibration{};
    const EncoderCalibrationStorage::LoadResult loadResult =
            encoderCalibrationStorage.Load(kMotorPolePairs, storedCalibration);
    const bool usedStoredCalibration =
            (loadResult == EncoderCalibrationStorage::LoadResult::VALID);

    const bool focInitOk = usedStoredCalibration
            ? focMotor.Init(storedCalibration.zeroElectricAngleOffset,
                            static_cast<EncoderBase::Direction>(storedCalibration.sensorDirection))
            : focMotor.Init();

    bool calibrationSaved = usedStoredCalibration;
    if (focInitOk && !usedStoredCalibration)
    {
        EncoderCalibrationStorage::Data freshCalibration{};
        freshCalibration.zeroElectricAngleOffset = focMotor.zeroElectricAngleOffset;
        freshCalibration.sensorDirection = static_cast<int32_t>(mt6816.countDirection);
        calibrationSaved = encoderCalibrationStorage.Save(kMotorPolePairs, freshCalibration);
    }

    // Entering position mode must not create an implicit step just because the
    // fixed startup target differs from the actual shaft position. Hold the
    // initialized multi-turn position until the host explicitly sets a target.
    if (focInitOk && focMotor.config.controlMode == Motor::ANGLE)
    {
        focMotor.target = focMotor.GetLastEstimateAngle();
    }

    Respond(*uart3StreamOutputPtr,
            "[foc] init=%d err=%d record=%s align=%s stored=%d linearity=%d noMag=%d csum=%d target=%.3f",
            focInitOk ? 1 : 0,
            static_cast<int>(focMotor.error),
            CalibrationLoadResultToString(loadResult),
            usedStoredCalibration ? "flash" : "fresh",
            calibrationSaved ? 1 : 0,
            mt6816.IsCalibrated() ? 1 : 0,
            mt6816.IsNoMagnetDetected() ? 1 : 0,
            mt6816.IsChecksumValid() ? 1 : 0,
            (double)focMotor.target);

    if (focInitOk)
    {
        // Alignment may use the power stage, but normal operation must remain
        // off until an explicit !START command is received after calibration.
        focMotor.SetEnable(false);
        // Alignment is done (used blocking encoder reads); switch the encoder to
        // the non-blocking pipeline so the high-rate FOC loop never waits on SPI.
        mt6816.EnableAsyncRead(true);
        gFocTickEnabled = true;
        motor->SetReady(true);
        Respond(*uart3StreamOutputPtr, "[foc] ready, waiting for !START");
    }
    else
    {
        motor->SetReady(false);
        gFocTickEnabled = false;
        Respond(*uart3StreamOutputPtr,
                "[foc] init failed, tim1_state=%d arr=%lu",
                (int)htim1.State,
                (unsigned long)__HAL_TIM_GET_AUTORELOAD(&htim1));
    }

    for (;;)
    {
        // Control loop is triggered by ADC injected conversion complete interrupt.
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (gFocTickEnabled) focMotor.Tick();
    }
}

static void ThreadVofa(void* argument)
{
    (void)argument;

    // 28 floats + 4-byte JustFloat tail = 116 bytes/frame. At 500 Hz this is
    // 58 kB/s, about 63% of a 921600-baud 8N1 UART's payload capacity.
    constexpr uint32_t kVofaPeriodMs = 2U;
    constexpr size_t kVofaChannelCount = 28U;
    float channels[kVofaChannelCount] = {0.0f};

    for (;;)
    {
        vofaDebug.SetOutput(uart3StreamOutputPtr);

        const PhaseCurrent_t phaseCurrent = currentSense.GetLastPhaseCurrents();
        const AlphaBetaCurrent_t alphaBetaCurrent = currentSense.GetLastAlphaBetaCurrents();
        const DqCurrent_t dqCurrent = focMotor.GetLastDqCurrent();
        const Motor::MotionTelemetry_t motion = focMotor.GetMotionTelemetry();

        // Channel order:
        // Existing signals:
        // dutyA, dutyB, dutyC, iA, iB, iC, iAlpha, iBeta, iq, id,
        // position, velocity, target, adcRawIa, adcRawIb, adcRawIc.
        // Motion/RTOS diagnostics:
        // trajectoryPosition, trajectoryVelocity, trajectoryAcceleration,
        // velocityCommand, currentCommand, positionError, velocityError,
        // controlLimitFlags, focStackFreeBytes, vofaStackFreeBytes,
        // rawVelocity, frictionCurrent.
        channels[0] = focDriver.dutyA;
        channels[1] = focDriver.dutyB;
        channels[2] = focDriver.dutyC;
        channels[3] = phaseCurrent.a;
        channels[4] = phaseCurrent.b;
        channels[5] = phaseCurrent.c;
        channels[6] = alphaBetaCurrent.iAlpha;
        channels[7] = alphaBetaCurrent.iBeta;
        channels[8] = dqCurrent.q;
        channels[9] = dqCurrent.d;
        channels[10] = focMotor.GetLastEstimateAngle();
        channels[11] = focMotor.GetLastEstimateVelocity();
        channels[12] = focMotor.target;
        channels[13] = static_cast<float>(currentSense.rawAdcVal[0]);
        channels[14] = static_cast<float>(currentSense.rawAdcVal[1]);
        channels[15] = static_cast<float>(currentSense.rawAdcVal[2]);
        channels[16] = motion.trajectoryPosition;
        channels[17] = motion.trajectoryVelocity;
        channels[18] = motion.trajectoryAcceleration;
        channels[19] = motion.velocityCommand;
        channels[20] = motion.currentCommand;
        channels[21] = motion.positionError;
        channels[22] = motion.velocityError;
        channels[23] = static_cast<float>(motion.limitFlags);
        channels[24] = focControlTaskHandle
                ? static_cast<float>(
                        uxTaskGetStackHighWaterMark(
                                static_cast<TaskHandle_t>(focControlTaskHandle)) *
                        sizeof(StackType_t))
                : 0.0f;
        channels[25] = static_cast<float>(
                uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t));
        channels[26] = focMotor.state.rawVelocity;
        channels[27] = motion.frictionCurrent;

        (void)vofaDebug.SendJustFloat(channels, kVofaChannelCount);
        osDelay(kVofaPeriodMs);
    }
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
        .stack_size = 1792,
        .priority = (osPriority_t)osPriorityRealtime,
    };
    focControlTaskHandle = osThreadNew(ThreadFocControl, nullptr, &focControlTask_attributes);
    if (focControlTaskHandle == nullptr)
    {
        ReportTaskCreateFailure("focControlTask");
    }

    const osThreadAttr_t peripheralTask_attributes = {
        .name = "peripheralTask",
        .stack_size = 1024,
        .priority = (osPriority_t)osPriorityNormal,
    };
    peripheralTaskHandle = osThreadNew(ThreadPeripheral, nullptr, &peripheralTask_attributes);
    if (peripheralTaskHandle == nullptr)
    {
        ReportTaskCreateFailure("peripheralTask");
    }

    const osThreadAttr_t vofaTask_attributes = {
        .name = "vofaTask",
        // The channel array grows by 48 bytes; SendJustFloat still uses its
        // fixed 132-byte frame buffer. Keep extra headroom and publish the live
        // high-water mark in VOFA channels 25/26.
        .stack_size = 1280,
        .priority = (osPriority_t)osPriorityBelowNormal,
    };
    vofaTaskHandle = osThreadNew(ThreadVofa, nullptr, &vofaTask_attributes);
    if (vofaTaskHandle == nullptr)
    {
        ReportTaskCreateFailure("vofaTask");
    }
}
