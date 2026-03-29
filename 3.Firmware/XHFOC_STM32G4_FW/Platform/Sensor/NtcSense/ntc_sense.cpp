#include "ntc_sense.h"
#include "adc.h"

void NtcSense::Init()
{
}

uint16_t NtcSense::GetRaw()
{
    raw_ = AdcGetRaw(ADC_SIGNAL_NTC);
    return raw_;
}

float NtcSense::GetVoltage()
{
    raw_ = AdcGetRaw(ADC_SIGNAL_NTC);
    return AdcRawToVoltage(raw_);
}

float NtcSense::GetTemperatureC()
{
    raw_ = AdcGetRaw(ADC_SIGNAL_NTC);
    return voltageToTemperatureC(AdcRawToVoltage(raw_));
}
