#ifndef XHFOC_NTC_SENSE_BASE_H
#define XHFOC_NTC_SENSE_BASE_H

#include <cstdint>

class NtcSenseBase
{
public:
    virtual ~NtcSenseBase() = default;

    virtual void Init() = 0;
    virtual uint16_t GetRaw() = 0;
    virtual float GetVoltage() = 0;
    virtual float GetTemperatureC() = 0;

protected:
    float voltageToTemperatureC(float voltage) const;

    float vRef_ = 3.3f;
    float pullupOhm_ = 10000.0f;
    float ntcNominalOhm_ = 10000.0f;
    float beta_ = 3950.0f;
    float nominalTempC_ = 25.0f;
};

#endif
