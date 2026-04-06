#include "current_sense.h"
#include "adc.h"

void CurrentSense::InitAdc()
{
    // ADC DMA is started globally in AdcStartDmaSampling().
}

float CurrentSense::GetAdcToVoltage(Channel_t channel)
{
    if (channel == CH_A)
    {
        uint16_t ia = 0;
        uint16_t ib = 0;
        uint16_t ic = 0;
        if (AdcGetInjectedPhaseCurrentsRaw(&ia, &ib, &ic))
        {
            rawAdcVal[CH_A] = ia;
            rawAdcVal[CH_B] = ib;
            rawAdcVal[CH_C] = ic;
        }
        else
        {
            rawAdcVal[CH_A] = AdcGetRaw(ADC_SIGNAL_IA);
            rawAdcVal[CH_B] = AdcGetRaw(ADC_SIGNAL_IB);
            rawAdcVal[CH_C] = AdcGetRaw(ADC_SIGNAL_IC);
        }
    }

    if (channel > CH_C)
    {
        channel = CH_A;
    }

    return AdcRawToVoltage(rawAdcVal[channel]);
}
