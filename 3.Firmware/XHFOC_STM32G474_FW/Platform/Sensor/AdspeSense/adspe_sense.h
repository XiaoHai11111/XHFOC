#ifndef XHFOC_ADSPE_SENSE_STM32_H
#define XHFOC_ADSPE_SENSE_STM32_H

#include "adspe_sense_base.h"

class AdspeSense : public AdspeSenseBase
{
public:
    void Init() override;
    uint16_t GetRaw() override;
    float GetVoltage() override;

private:
    uint16_t raw_ = 0;
};

#endif
