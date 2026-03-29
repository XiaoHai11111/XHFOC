#ifndef XHFOC_ADSPE_SENSE_BASE_H
#define XHFOC_ADSPE_SENSE_BASE_H

#include <cstdint>

class AdspeSenseBase
{
public:
    virtual ~AdspeSenseBase() = default;

    virtual void Init() = 0;
    virtual uint16_t GetRaw() = 0;
    virtual float GetVoltage() = 0;
};

#endif
