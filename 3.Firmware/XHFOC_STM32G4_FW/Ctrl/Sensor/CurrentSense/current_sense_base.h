#ifndef XHFOC_CURRENT_SENSE_BASE_H
#define XHFOC_CURRENT_SENSE_BASE_H

#include <cstdint>
#include "math_utils.h"

class CurrentSenseBase
{
public:
    //static constexpr uint32_t ADC_RESOLUTION = 4096;
    //static constexpr float ADC_VOLTAGE_RANGE = 3.3f;

    virtual ~CurrentSenseBase() = default;

    virtual void Init() = 0;
    virtual PhaseCurrent_t GetPhaseCurrents() = 0;
    DqCurrent_t GetFocCurrents(float angleElectrical);

protected:
    float iAlpha = 0.0f;
    float iBeta = 0.0f;
};

#endif
