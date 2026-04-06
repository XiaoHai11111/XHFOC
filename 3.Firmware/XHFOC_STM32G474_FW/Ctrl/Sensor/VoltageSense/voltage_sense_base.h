#ifndef XHFOC_VOLTAGE_SENSE_BASE_H
#define XHFOC_VOLTAGE_SENSE_BASE_H

struct PhaseVoltage_t
{
    float a = 0.0f;
    float b = 0.0f;
    float c = 0.0f;
};

class VoltageSenseBase
{
public:
    virtual ~VoltageSenseBase() = default;

    virtual void Init() = 0;
    virtual PhaseVoltage_t GetPhaseVoltages() = 0;
    virtual float GetBusVoltage() = 0;
};

#endif
