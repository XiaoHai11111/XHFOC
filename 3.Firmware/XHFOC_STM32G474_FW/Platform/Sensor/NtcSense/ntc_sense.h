#ifndef XHFOC_NTC_SENSE_STM32_H
#define XHFOC_NTC_SENSE_STM32_H

#include "ntc_sense_base.h"

class NtcSense : public NtcSenseBase
{
public:
    void Init() override;
    uint16_t GetRaw() override;
    float GetVoltage() override;
    float GetTemperatureC() override;

private:
    uint16_t raw_ = 0;
};

#endif
