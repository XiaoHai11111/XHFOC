#include "low_side_current_sense.h"
#include "ascii_processor.hpp"
#include "interface_uart.hpp"

LowSideCurrentSenseBase::LowSideCurrentSenseBase(float shuntResistorIn, float gain)
{
    shuntResistor = shuntResistorIn;
    ampGain = gain;

    voltageToAmpRatio = shuntResistor * ampGain;

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

    // Use the exact same acquisition path as runtime (RefreshAdcSample -> rawAdcVal)
    // so the phase<->ADC-rank mapping can never diverge from GetPhaseCurrents().
    for (int i = 0; i < calibrationRounds; ++i)
    {
        HAL_Delay(1);
        RefreshAdcSample();
        zeroOffsetA += GetAdcToVoltage(rawAdcVal[CH_A]);
        zeroOffsetB += GetAdcToVoltage(rawAdcVal[CH_B]);
        zeroOffsetC += GetAdcToVoltage(rawAdcVal[CH_C]);
    }

    zeroOffsetA /= (float)calibrationRounds;
    zeroOffsetB /= (float)calibrationRounds;
    zeroOffsetC /= (float)calibrationRounds;

    if (uart3StreamOutputPtr != nullptr)
    {
        Respond(*uart3StreamOutputPtr,
                "[curr] zeroOffsetA=%fV zeroOffsetB=%fV zeroOffsetC=%fV",
                zeroOffsetA,
                zeroOffsetB,
                zeroOffsetC);
    }
}

PhaseCurrent_t LowSideCurrentSenseBase::GetPhaseCurrents()
{
    RefreshAdcSample();

    PhaseCurrent_t current{};

    // Two-shunt sensing: the phase-A current-sense channel is unusable on this
    // board (amplifier output railed near ADC full-scale even at zero current),
    // so only phases B and C are measured and A is reconstructed from
    // Kirchhoff's current law (iA + iB + iC = 0).
    current.b = (GetAdcToVoltage(rawAdcVal[CH_B]) - zeroOffsetB) / gainB;
    current.c = (GetAdcToVoltage(rawAdcVal[CH_C]) - zeroOffsetC) / gainC;
    current.a = -(current.b + current.c);

    return current;
}
