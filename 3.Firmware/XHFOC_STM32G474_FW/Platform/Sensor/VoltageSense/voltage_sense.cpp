#include "voltage_sense.h"
#include "adc.h"

void VoltageSense::Init()
{
}

PhaseVoltage_t VoltageSense::GetPhaseVoltages()
{
    rawVa_ = AdcGetRaw(ADC_SIGNAL_VA);
    rawVb_ = AdcGetRaw(ADC_SIGNAL_VB);
    rawVc_ = AdcGetRaw(ADC_SIGNAL_VC);

    PhaseVoltage_t v{};
    v.a = AdcRawToVoltage(rawVa_) * phaseRatioA_;
    v.b = AdcRawToVoltage(rawVb_) * phaseRatioB_;
    v.c = AdcRawToVoltage(rawVc_) * phaseRatioC_;
    return v;
}

float VoltageSense::GetBusVoltage()
{
    rawVbus_ = AdcGetRaw(ADC_SIGNAL_VBUS);
    return AdcRawToVoltage(rawVbus_) * busRatio_;
}

void VoltageSense::SetPhaseDivider(float ratioA, float ratioB, float ratioC)
{
    phaseRatioA_ = ratioA;
    phaseRatioB_ = ratioB;
    phaseRatioC_ = ratioC;
}

void VoltageSense::SetBusDivider(float ratioBus)
{
    busRatio_ = ratioBus;
}
