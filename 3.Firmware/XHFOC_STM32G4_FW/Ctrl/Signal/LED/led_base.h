#ifndef CTRL_STEP_FW_LED_BASE_H
#define CTRL_STEP_FW_LED_BASE_H

#include <cstdint>
#include "motor.h"

class LedBase
{
public:
    LedBase()
    = default;

    void Tick(uint32_t _timeElapseMillis, Motor::RunState_t _state);

private:
    enum LedId : uint8_t
    {
        RED = 0,
        GREEN = 1,
        YELLOW = 2
    };
    uint32_t timerMillis = 0;

    bool IsBurstBlinkOn(uint8_t blinkCount, uint16_t onMs, uint16_t offMs, uint16_t idleMs) const;
    bool IsHeartbeatOn() const;

    virtual void SetLedState(uint8_t _id, bool _state) = 0;
};


#endif
