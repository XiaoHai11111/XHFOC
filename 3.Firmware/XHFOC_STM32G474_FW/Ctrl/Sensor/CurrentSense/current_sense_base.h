#ifndef XHFOC_CURRENT_SENSE_BASE_H
#define XHFOC_CURRENT_SENSE_BASE_H

#include <cstdint>
#include "math_utils.h"

class CurrentSenseBase
{
public:
    static constexpr uint16_t ADC_CONVERT_TIME_US = 8;
    static constexpr uint32_t ADC_RESOLUTION = 4096;
    static constexpr float ADC_VOLTAGE_RANGE = 3.3f;

    uint8_t sector = 1;
    float pwmDutyA = 0.0f;
    float pwmDutyB = 0.0f;
    float pwmDutyC = 0.0f;
    float dropOnePhaseDutyThreshold = 0.75f;

    virtual ~CurrentSenseBase() = default;

    virtual void Init() = 0;
    virtual PhaseCurrent_t GetPhaseCurrents() = 0;
    virtual float GetDcCurrent(float angleElectrical);
    DqCurrent_t GetFocCurrents(float angleElectrical);

protected:
    float iAlpha = 0.0f;
    float iBeta = 0.0f;
};

#endif
