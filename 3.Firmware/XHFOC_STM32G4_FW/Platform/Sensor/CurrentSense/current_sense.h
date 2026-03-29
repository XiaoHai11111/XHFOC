#ifndef XHFOC_CURRENT_SENSE_STM32_H
#define XHFOC_CURRENT_SENSE_STM32_H

#include "low_side_current_sense.h"
#include <cstdint>

class CurrentSense : public LowSideCurrentSenseBase
{
public:
    explicit CurrentSense(float shuntResistor, float gain)
        : LowSideCurrentSenseBase(shuntResistor, gain)
    {}

    uint16_t rawAdcVal[3] = {0, 0, 0};

private:
    void InitAdc() override;
    float GetAdcToVoltage(Channel_t channel) override;
};

#endif
