#include "current_sense.h"
#include "adc.h"

void CurrentSense::InitAdc()
{
    // ADC DMA is started globally in AdcStartDmaSampling().
}

void CurrentSense::RefreshAdcSample()
{
    uint16_t ia = 0;
    uint16_t ib = 0;
    uint16_t ic = 0;

    // Use synchronized injected sampling for all three phases.
    if (AdcGetInjectedPhaseCurrentsRaw(&ia, &ib, &ic))
    {
        rawAdcVal[CH_A] = ia;
        rawAdcVal[CH_B] = ib;
        rawAdcVal[CH_C] = ic;
    }
}

float CurrentSense::GetAdcToVoltage(uint16_t raw)
{
    return AdcRawToVoltage(raw);
}
