#include "current_sense.h"
#include "adc.h"

void CurrentSense::InitAdc()
{
    // ADC DMA is started globally in AdcStartDmaSampling().
}

float CurrentSense::GetAdcToVoltage(Channel_t channel)
{
    AdcSignal_t signal = ADC_SIGNAL_IA;
    switch (channel)
    {
        case CH_A: signal = ADC_SIGNAL_IA; break;
        case CH_B: signal = ADC_SIGNAL_IB; break;
        case CH_C: signal = ADC_SIGNAL_IC; break;
        default: signal = ADC_SIGNAL_IA; break;
    }

    rawAdcVal[channel] = AdcGetRaw(signal);
    return AdcRawToVoltage(rawAdcVal[channel]);
}
