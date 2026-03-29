#ifndef CTRL_FOC_LITE_FW_MOTOR_H
#define CTRL_FOC_LITE_FW_MOTOR_H

#include "encoder_base.h"
#include "math_utils.h"

class Motor
{
public:
    explicit Motor(int _polePairs, float _phaseResistance = NOT_SET) :
        polePairs(_polePairs), phaseResistance(_phaseResistance)
    {
        config.controlMode = ANGLE;
        config.voltageUsedForSensorAlign =1.0f;
        config.voltageLimit = 12.0f;
        config.currentLimit = 0.2f;
        config.velocityLimit = 20.0f;

    }


    enum ControlMode_t
    {
        TORQUE,
        VELOCITY,
        ANGLE,
        VELOCITY_OPEN_LOOP,
        ANGLE_OPEN_LOOP
    };

    enum Error_t
    {
        NO_ERROR = 0,
        FAILED_TO_NOTICE_MOVEMENT,
        POLE_PAIR_MISMATCH
    };

    struct Config_t
    {
        float voltageLimit{};
        float currentLimit{};
        float velocityLimit{};
        float voltageUsedForSensorAlign{};
        ControlMode_t controlMode = ANGLE;
    };

    struct State_t
    {
        float rawAngle{};
        float estAngle{};
        float rawVelocity{};
        float estVelocity{};
    };

    typedef enum
    {
        STATE_STOP,
        STATE_FINISH,
        STATE_RUNNING,
        STATE_OVERLOAD,
        STATE_STALL,
        STATE_NO_CALIB
    } RunState_t;

    bool Init(float _zeroElectricOffset = NOT_SET, EncoderBase::Direction _encoderDir = EncoderBase::CW);
    void AttachEncoder(EncoderBase* _encoder);
    void SetEnable(bool _enable);
    float GetEstimateAngle();
    float GetEstimateVelocity();
    float GetElectricalAngle();
    void Tick();
    void SetTorqueLimit(float _val);


    float target = 0;
    Error_t error = NO_ERROR;
    Config_t config{};
    State_t state{};
    DqVoltage_t voltage{};
    DqCurrent_t current{};
    float zeroElectricAngleOffset = NOT_SET;
    EncoderBase* encoder = nullptr;


private:
    bool InitFOC(float _zeroElectricOffset, EncoderBase::Direction _sensorDirection);
    bool AlignSensor();
    void CloseLoopControlTick();
    void FocOutputTick();
    float VelocityOpenLoopTick(float _target);
    float AngleOpenLoopTick(float _target);
    void SetPhaseVoltage(float _voltageQ, float _voltageD, float _angleElectrical);


    bool enabled = false;
    float phaseResistance = NOT_SET;
    int polePairs = 7;
    float voltageA{}, voltageB{}, voltageC{};
    float estimateAngle{};
    float electricalAngle{};
    float estimateVelocity{};
    float setPointCurrent{};
    float setPointVelocity{};
    float setPointAngle{};
    uint64_t openLoopTimestamp{};
};


#endif
