#include "adspe_sense.h"
#include "adc.h"

void AdspeSense::Init()
{
}

uint16_t AdspeSense::GetRaw()
{
    raw_ = AdcGetRaw(ADC_SIGNAL_ADSPE);
    return raw_;
}

float AdspeSense::GetVoltage()
{
    raw_ = AdcGetRaw(ADC_SIGNAL_ADSPE);
    return AdcRawToVoltage(raw_);
}
