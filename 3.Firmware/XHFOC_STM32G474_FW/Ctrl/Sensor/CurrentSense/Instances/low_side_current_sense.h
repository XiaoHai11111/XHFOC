#ifndef XHFOC_LOW_SIDE_CURRENT_SENSE_H
#define XHFOC_LOW_SIDE_CURRENT_SENSE_H

#include "stm32g4xx_hal.h"
#include "current_sense_base.h"

class LowSideCurrentSenseBase : public CurrentSenseBase
{
public:
    enum Channel_t
    {
        CH_A = 0,
        CH_B = 1,
        CH_C = 2
    };

    LowSideCurrentSenseBase(float shuntResistor, float gain);

    void Init() override;
    PhaseCurrent_t GetPhaseCurrents() override;

    float gainA = 0.0f;
    float gainB = 0.0f;
    float gainC = 0.0f;

protected:
    virtual void InitAdc() = 0;
    virtual float GetAdcToVoltage(Channel_t channel) = 0;

private:
    float shuntResistor = 0.0f;
    float ampGain = 0.0f;
    float voltageToAmpRatio = 0.0f;

    void CalibrateOffsets();
    float zeroOffsetA = 0.0f;
    float zeroOffsetB = 0.0f;
    float zeroOffsetC = 0.0f;
};

#endif
