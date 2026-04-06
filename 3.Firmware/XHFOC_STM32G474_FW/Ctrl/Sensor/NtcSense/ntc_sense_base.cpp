#include "ntc_sense_base.h"
#include <cmath>

float NtcSenseBase::voltageToTemperatureC(float voltage) const
{
    if (voltage <= 0.0f || voltage >= vRef_)
    {
        return -273.15f;
    }

    const float resistance = pullupOhm_ * voltage / (vRef_ - voltage);
    const float nominalTempK = nominalTempC_ + 273.15f;
    const float invTemp = (1.0f / nominalTempK) + (std::log(resistance / ntcNominalOhm_) / beta_);
    const float tempK = 1.0f / invTemp;
    return tempK - 273.15f;
}
