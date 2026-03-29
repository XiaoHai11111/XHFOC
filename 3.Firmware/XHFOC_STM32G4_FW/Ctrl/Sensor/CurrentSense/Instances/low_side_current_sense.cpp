#include "low_side_current_sense.h"

LowSideCurrentSenseBase::LowSideCurrentSenseBase(float shuntResistorIn, float gain)
{
    shuntResistor = shuntResistorIn;
    ampGain = gain;

    voltageToAmpRatio = 1.0f / shuntResistor / ampGain;

    gainA = voltageToAmpRatio;
    gainB = voltageToAmpRatio;
    gainC = voltageToAmpRatio;
}

void LowSideCurrentSenseBase::Init()
{
    InitAdc();
    CalibrateOffsets();
}

void LowSideCurrentSenseBase::CalibrateOffsets()
{
    const int calibrationRounds = 1000;

    zeroOffsetA = 0.0f;
    zeroOffsetB = 0.0f;
    zeroOffsetC = 0.0f;

    for (int i = 0; i < calibrationRounds; ++i)
    {
        zeroOffsetA += GetAdcToVoltage(CH_A);
        zeroOffsetB += GetAdcToVoltage(CH_B);
        zeroOffsetC += GetAdcToVoltage(CH_C);
        HAL_Delay(1);
    }

    zeroOffsetA /= (float)calibrationRounds;
    zeroOffsetB /= (float)calibrationRounds;
    zeroOffsetC /= (float)calibrationRounds;
}

PhaseCurrent_t LowSideCurrentSenseBase::GetPhaseCurrents()
{
    PhaseCurrent_t current{};
    current.a = (GetAdcToVoltage(CH_A) - zeroOffsetA) * gainA;
    current.b = (GetAdcToVoltage(CH_B) - zeroOffsetB) * gainB;
    current.c = (GetAdcToVoltage(CH_C) - zeroOffsetC) * gainC;
    return current;
}
