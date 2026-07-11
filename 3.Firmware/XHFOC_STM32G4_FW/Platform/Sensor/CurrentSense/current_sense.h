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

private:
    void InitAdc() override;
    void RefreshAdcSample() override;
    float GetAdcToVoltage(uint16_t raw) override;
};

#endif
