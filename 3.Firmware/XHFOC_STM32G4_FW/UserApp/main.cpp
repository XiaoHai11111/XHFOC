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
constexpr uint32_t kRuntimeStatsTaskCapacity = 10U;
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
static volatile bool gFocTimingArmed = false;
static volatile uint32_t gAdcIrqEntryCycle = 0U;
static volatile uint32_t gAdcEventCount = 0U;
static volatile uint32_t gAdcHandledCount = 0U;
static volatile uint32_t gFirstPendingAdcCycle = 0U;
static volatile uint32_t gFocTickCount = 0U;
static volatile uint32_t gFocMissedTickCount = 0U;
static volatile uint32_t gFocTickMaxCycles = 0U;
static volatile uint32_t gAdcWakeMaxCycles = 0U;
VofaDebug vofaDebug;

extern "C" void OnFocAdcIrqEnterFromISR(void)
{
    gAdcIrqEntryCycle = DWT->CYCCNT;
}

extern "C" void OnFocTimerElapsedFromISR(void)
{
    if ((focControlTaskHandle == nullptr) || !gFocTimingArmed)
    {
        return;
    }

    const uint32_t eventCount = gAdcEventCount + 1U;
    gAdcEventCount = eventCount;
    if ((eventCount - gAdcHandledCount) == 1U)
    {
        gFirstPendingAdcCycle = gAdcIrqEntryCycle;
    }

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(TaskHandle_t(focControlTaskHandle), &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

struct TaskRuntimeHistory
{
    UBaseType_t taskNumber;
    uint32_t runtimeCounter;
};

static TaskRuntimeHistory gTaskRuntimeHistory[kRuntimeStatsTaskCapacity] = {};

static char TaskStateToChar(eTaskState state)
{
    switch (state)
    {
        case eRunning: return 'R';
        case eReady: return 'Y';
        case eBlocked: return 'B';
        case eSuspended: return 'S';
        case eDeleted: return 'D';
        default: return '?';
    }
}

static uint32_t UpdateTaskRuntimeHistory(const TaskStatus_t& status)
{
    TaskRuntimeHistory* emptySlot = nullptr;
    for (TaskRuntimeHistory& history : gTaskRuntimeHistory)
    {
        if (history.taskNumber == status.xTaskNumber)
        {
            const uint32_t delta = status.ulRunTimeCounter - history.runtimeCounter;
            history.runtimeCounter = status.ulRunTimeCounter;
            return delta;
        }
        if ((history.taskNumber == 0U) && (emptySlot == nullptr))
        {
            emptySlot = &history;
        }
    }

    if (emptySlot != nullptr)
    {
        emptySlot->taskNumber = status.xTaskNumber;
        emptySlot->runtimeCounter = status.ulRunTimeCounter;
    }
    return status.ulRunTimeCounter;
}

static void ReportRuntimeDiagnostics()
{
    uint32_t focTickCount;
    uint32_t missedTickCount;
    uint32_t tickMaxCycles;
    uint32_t wakeMaxCycles;

    taskENTER_CRITICAL();
    focTickCount = gFocTickCount;
    missedTickCount = gFocMissedTickCount;
    tickMaxCycles = gFocTickMaxCycles;
    wakeMaxCycles = gAdcWakeMaxCycles;
    taskEXIT_CRITICAL();

    const uint64_t tickTimeHundredthsUs =
            (static_cast<uint64_t>(tickMaxCycles) * 100000000ULL) / SystemCoreClock;
    const uint64_t wakeTimeHundredthsUs =
            (static_cast<uint64_t>(wakeMaxCycles) * 100000000ULL) / SystemCoreClock;

    Respond(*uart3StreamOutputPtr,
            "[rtos] uptime_ms=%lu foc_ticks=%lu missed=%lu tick_max=%lu cyc (%lu.%02lu us) "
            "wake_max=%lu cyc (%lu.%02lu us)",
            (unsigned long)HAL_GetTick(),
            (unsigned long)focTickCount,
            (unsigned long)missedTickCount,
            (unsigned long)tickMaxCycles,
            (unsigned long)(tickTimeHundredthsUs / 100ULL),
            (unsigned long)(tickTimeHundredthsUs % 100ULL),
            (unsigned long)wakeMaxCycles,
            (unsigned long)(wakeTimeHundredthsUs / 100ULL),
            (unsigned long)(wakeTimeHundredthsUs % 100ULL));

    const UBaseType_t taskCount = uxTaskGetNumberOfTasks();
    Respond(*uart3StreamOutputPtr,
            "[rtos] heap=%uB min_heap=%uB active_tasks=%lu capacity=%lu",
            (unsigned)xPortGetFreeHeapSize(),
            (unsigned)xPortGetMinimumEverFreeHeapSize(),
            (unsigned long)taskCount,
            (unsigned long)kRuntimeStatsTaskCapacity);

    if (taskCount > kRuntimeStatsTaskCapacity)
    {
        Respond(*uart3StreamOutputPtr,
                "[rtos] task_snapshot=overflow count=%lu capacity=%lu",
                (unsigned long)taskCount,
                (unsigned long)kRuntimeStatsTaskCapacity);
        return;
    }

    auto* taskStatus = static_cast<TaskStatus_t*>(
            pvPortMalloc(sizeof(TaskStatus_t) * kRuntimeStatsTaskCapacity));
    if (taskStatus == nullptr)
    {
        Respond(*uart3StreamOutputPtr, "[rtos] task_snapshot=no_heap");
        return;
    }

    const UBaseType_t capturedCount =
            uxTaskGetSystemState(taskStatus, kRuntimeStatsTaskCapacity, nullptr);
    if (capturedCount == 0U)
    {
        vPortFree(taskStatus);
        Respond(*uart3StreamOutputPtr, "[rtos] task_snapshot=failed");
        return;
    }

    uint64_t totalRuntimeDelta = 0ULL;
    for (UBaseType_t i = 0U; i < capturedCount; ++i)
    {
        taskStatus[i].ulRunTimeCounter = UpdateTaskRuntimeHistory(taskStatus[i]);
        totalRuntimeDelta += taskStatus[i].ulRunTimeCounter;
    }

    for (UBaseType_t i = 0U; i < capturedCount; ++i)
    {
        const uint32_t cpuPermille = (totalRuntimeDelta == 0ULL)
                ? 0U
                : static_cast<uint32_t>(
                        (static_cast<uint64_t>(taskStatus[i].ulRunTimeCounter) * 1000ULL) /
                        totalRuntimeDelta);
        const uint32_t stackMinFreeBytes =
                static_cast<uint32_t>(taskStatus[i].usStackHighWaterMark) *
                sizeof(StackType_t);

        Respond(*uart3StreamOutputPtr,
                "[rtos.task] name=%s cpu=%lu.%lu%% stack_min_free=%luB prio=%lu state=%c",
                taskStatus[i].pcTaskName,
                (unsigned long)(cpuPermille / 10U),
                (unsigned long)(cpuPermille % 10U),
                (unsigned long)stackMinFreeBytes,
                (unsigned long)taskStatus[i].uxCurrentPriority,
                TaskStateToChar(taskStatus[i].eCurrentState));
    }

    vPortFree(taskStatus);
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
    constexpr uint32_t kDiagnosticsPeriodMs = 10000U;
    uint32_t elapsedForKeys = 0;
    uint32_t elapsedInState = 0;
    uint32_t elapsedForLed = 0;
    uint32_t elapsedForDiagnostics = 0;
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

        elapsedForDiagnostics += kLoopMs;
        if (elapsedForDiagnostics >= kDiagnosticsPeriodMs)
        {
            elapsedForDiagnostics = 0U;
            ReportRuntimeDiagnostics();
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

    taskENTER_CRITICAL();
    gAdcEventCount = 0U;
    gAdcHandledCount = 0U;
    gFirstPendingAdcCycle = 0U;
    gFocTickCount = 0U;
    gFocMissedTickCount = 0U;
    gFocTickMaxCycles = 0U;
    gAdcWakeMaxCycles = 0U;
    (void)ulTaskNotifyTake(pdTRUE, 0U);
    gFocTimingArmed = focInitOk;
    taskEXIT_CRITICAL();

    for (;;)
    {
        // Control loop is triggered by ADC injected conversion complete interrupt.
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        const uint32_t taskWakeCycle = DWT->CYCCNT;

        taskENTER_CRITICAL();
        const uint32_t adcEventCount = gAdcEventCount;
        const uint32_t pendingTickCount = adcEventCount - gAdcHandledCount;
        const uint32_t firstPendingAdcCycle = gFirstPendingAdcCycle;
        gAdcHandledCount = adcEventCount;
        (void)ulTaskNotifyTake(pdTRUE, 0U);
        taskEXIT_CRITICAL();

        if (pendingTickCount == 0U)
        {
            continue;
        }

        const uint32_t wakeCycles = taskWakeCycle - firstPendingAdcCycle;
        if (wakeCycles > gAdcWakeMaxCycles)
        {
            gAdcWakeMaxCycles = wakeCycles;
        }
        if (pendingTickCount > 1U)
        {
            gFocMissedTickCount += pendingTickCount - 1U;
        }

        if (gFocTickEnabled)
        {
            const uint32_t tickStartCycle = DWT->CYCCNT;
            focMotor.Tick();
            const uint32_t tickCycles = DWT->CYCCNT - tickStartCycle;
            if (tickCycles > gFocTickMaxCycles)
            {
                gFocTickMaxCycles = tickCycles;
            }
            gFocTickCount++;
        }
    }
}

static void ThreadVofa(void* argument)
{
    (void)argument;

    constexpr uint32_t kVofaPeriodMs = 1U; // 500Hz
    constexpr size_t kVofaChannelCount = 16U;
    float channels[kVofaChannelCount] = {0.0f};

    for (;;)
    {
        vofaDebug.SetOutput(uart3StreamOutputPtr);

        const PhaseCurrent_t phaseCurrent = currentSense.GetLastPhaseCurrents();
        const AlphaBetaCurrent_t alphaBetaCurrent = currentSense.GetLastAlphaBetaCurrents();
        const DqCurrent_t dqCurrent = focMotor.GetLastDqCurrent();

        // Channel order:
        // dutyA, dutyB, dutyC, iA, iB, iC, iAlpha, iBeta, iq, id, position, velocity, target, adcRawIa, adcRawIb, adcRawIc
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
        .stack_size = 2048,
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
        .stack_size = 1024,
        .priority = (osPriority_t)osPriorityBelowNormal,
    };
    vofaTaskHandle = osThreadNew(ThreadVofa, nullptr, &vofaTask_attributes);
    if (vofaTaskHandle == nullptr)
    {
        ReportTaskCreateFailure("vofaTask");
    }
}
