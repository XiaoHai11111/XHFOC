#include "led_base.h"

namespace {
constexpr uint32_t kPatternPeriodMs = 800U;
}

bool LedBase::IsBurstBlinkOn(uint8_t blinkCount, uint16_t onMs, uint16_t offMs, uint16_t idleMs) const
{
    if (blinkCount == 0)
        return false;
    const uint32_t burstLength = static_cast<uint32_t>(blinkCount) * (onMs + offMs);
    const uint32_t period = burstLength + idleMs;
    const uint32_t phase = timerMillis % period;
    if (phase >= burstLength)
        return false;

    const uint32_t pairPhase = phase % (onMs + offMs);
    return pairPhase < onMs;
}

bool LedBase::IsHeartbeatOn() const
{
    const uint32_t phase = timerMillis % kPatternPeriodMs;
    return (phase < 80U) || (phase >= 160U && phase < 240U);
}

void LedBase::Tick(uint32_t _timeElapseMillis, Motor::RunState_t _state)
{
    timerMillis += _timeElapseMillis;

    bool redOn = false;
    bool greenOn = false;
    bool yellowOn = false;

    switch (_state)
    {
        case Motor::STATE_NO_CALIB:
            // 1 pulse + long off, total 800ms.
            yellowOn = IsBurstBlinkOn(1, 160, 80, 560);
            break;
        case Motor::STATE_RUNNING:
            // Double heartbeat pulse in 800ms.
            greenOn = IsHeartbeatOn();
            break;
        case Motor::STATE_FINISH:
            greenOn = true;
            break;
        case Motor::STATE_STOP:
            // 800ms single short pulse.
            yellowOn = ((timerMillis % kPatternPeriodMs) < 120U);
            break;
        case Motor::STATE_OVERLOAD:
            // 3 fast pulses in 800ms.
            redOn = IsBurstBlinkOn(3, 80, 60, 380);
            break;
        case Motor::STATE_STALL:
            // 2 pulses in 800ms.
            redOn = IsBurstBlinkOn(2, 120, 80, 400);
            break;
    }

    SetLedState(RED, redOn);
    SetLedState(GREEN, greenOn);
    SetLedState(YELLOW, yellowOn);
}
