#ifndef XHFOC_VOLTAGE_SENSE_STM32_H
#define XHFOC_VOLTAGE_SENSE_STM32_H

#include "voltage_sense_base.h"
#include <cstdint>

class VoltageSense : public VoltageSenseBase
{
public:
    void Init() override;
    PhaseVoltage_t GetPhaseVoltages() override;
    float GetBusVoltage() override;

    uint16_t GetRawVA() const { return rawVa_; }
    uint16_t GetRawVB() const { return rawVb_; }
    uint16_t GetRawVC() const { return rawVc_; }
    uint16_t GetRawVBUS() const { return rawVbus_; }

    void SetPhaseDivider(float ratioA, float ratioB, float ratioC);
    void SetBusDivider(float ratioBus);

private:
    float phaseRatioA_ = 1.0f;
    float phaseRatioB_ = 1.0f;
    float phaseRatioC_ = 1.0f;
    float busRatio_ = 1.0f;

    uint16_t rawVa_ = 0;
    uint16_t rawVb_ = 0;
    uint16_t rawVc_ = 0;
    uint16_t rawVbus_ = 0;
};

#endif
