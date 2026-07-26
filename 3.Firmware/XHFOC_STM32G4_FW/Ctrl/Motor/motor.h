#ifndef CTRL_FOC_LITE_FW_MOTOR_H
#define CTRL_FOC_LITE_FW_MOTOR_H

#include "driver_base.h"
#include "encoder_base.h"
#include "current_sense_base.h"
#include "lowpass_filter.h"
#include "math_utils.h"
#include "pid.h"

class Motor
{
public:
    explicit Motor(int _polePairs) :polePairs(_polePairs)
    {
        config.controlMode = TORQUE;
        config.voltageUsedForSensorAlign =2.0f;
        config.voltageLimit = 12.0f;
        config.currentLimit = 10.0f;
        config.velocityLimit = 500.0f;
        config.positionVelocityLimit = 6.0f;
        config.positionVelocitySafetyLimit = 8.0f;
        config.positionAccelerationLimit = 12.0f;
        config.positionDecelerationLimit = 18.0f;
        config.positionCurrentLimit = 1.2f;
        config.positionAccelerationCurrentGain = 0.005f;
        config.positionFrictionCurrent = 0.22f;
        config.positionStictionCurrent = 0.35f;
        config.positionDeadband = 0.005f;

        config.lpfCurrentQ = LowPassFilter{0.0005f};
        config.lpfCurrentD = LowPassFilter{0.0005f};
        config.lpfVelocity = LowPassFilter{0.012f};
        config.lpfAngle = LowPassFilter{0.003f};

        config.pidCurrentQ = PidController{0.015f, 35.0f, 0.0f, 0.0f, 12.0f};
        config.pidCurrentD = PidController{0.01f, 20.0f, 0.0f, 0.0f, 12.0f};
        config.pidVelocity = PidController{0.10f, 0.0f, 0.0f, 80.0f, 12.0f};
        config.pidAngle = PidController{3.0f, 0, 0, 0, 10.0f};
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
        float positionVelocityLimit{};
        float positionVelocitySafetyLimit{};
        float positionAccelerationLimit{};
        float positionDecelerationLimit{};
        float positionCurrentLimit{};
        float positionAccelerationCurrentGain{};
        float positionFrictionCurrent{};
        float positionStictionCurrent{};
        float positionDeadband{};
        float voltageUsedForSensorAlign{};
        ControlMode_t controlMode = ANGLE;
        LowPassFilter lpfCurrentQ{};
        LowPassFilter lpfCurrentD{};
        LowPassFilter lpfVelocity{};
        LowPassFilter lpfAngle{};
        PidController pidCurrentQ;
        PidController pidCurrentD;
        PidController pidVelocity;
        PidController pidAngle;
    };

    struct State_t
    {
        float rawAngle{};
        float estAngle{};
        float rawVelocity{};
        float estVelocity{};
    };

    struct MotionTelemetry_t
    {
        float trajectoryPosition{};
        float trajectoryVelocity{};
        float trajectoryAcceleration{};
        float velocityCommand{};
        float currentCommand{};
        float positionError{};
        float velocityError{};
        float frictionCurrent{};
        uint32_t limitFlags{};
    };

    enum LimitFlag_t : uint32_t
    {
        LIMIT_NONE = 0U,
        LIMIT_TRAJECTORY_VELOCITY = 1U << 0,
        LIMIT_VELOCITY_COMMAND = 1U << 1,
        LIMIT_POSITION_CURRENT = 1U << 2,
        LIMIT_VOLTAGE = 1U << 3
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
    void AttachDriver(DriverBase* _driver);
    void AttachEncoder(EncoderBase* _encoder);
    void AttachCurrentSense(CurrentSenseBase* _currentSense);
    void SetEnable(bool _enable);
    float GetEstimateAngle();
    float GetEstimateVelocity();
    float GetElectricalAngle();
    void Tick();
    void SetTorqueLimit(float _val);
    void SetControlLoopHz(float _hz);
    float GetLastEstimateAngle() const;
    float GetLastEstimateVelocity() const;
    DqCurrent_t GetLastDqCurrent() const;
    MotionTelemetry_t GetMotionTelemetry() const;


    float target = 0;
    Error_t error = NO_ERROR;
    Config_t config{};
    State_t state{};
    DqVoltage_t voltage{};
    DqCurrent_t current{};
    AlphaBetaVoltage_t alphaBetaVoltage{};
    float zeroElectricAngleOffset = NOT_SET;
    CurrentSenseBase* currentSense = nullptr;
    DriverBase* driver = nullptr;
    EncoderBase* encoder = nullptr;


private:
    bool InitFOC(float _zeroElectricOffset, EncoderBase::Direction _sensorDirection);
    bool AlignSensor();
    void CloseLoopControlTick();
    void FocOutputTick();
    void ResetPositionTrajectory(float position);
    void PlanPositionTrajectory(float targetPosition);
    void UpdatePositionTrajectory(float dt);
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
    float setPointCurrentTarget{};
    float setPointVelocity{};
    float setPointAngle{};
    uint64_t openLoopTimestamp{};
    float controlLoopDeltaT_ = 0.0f;
    uint16_t velocityLoopDecimation_ = 1;
    uint16_t velocityLoopCounter_ = 0;
    uint16_t positionLoopDecimation_ = 1;
    uint16_t positionLoopCounter_ = 0;
    float currentSetpointRamp_ = 600.0f; // A/s

    MotionTelemetry_t motionTelemetry_{};
    bool trajectoryPlanned_ = false;
    float trajectoryTarget_ = 0.0f;
    float trajectoryElapsed_ = 0.0f;
    float trajectoryTotalTime_ = 0.0f;
    float trajectoryAccelTime_ = 0.0f;
    float trajectoryCruiseTime_ = 0.0f;
    float trajectoryDecelTime_ = 0.0f;
    float trajectoryInitialPosition_ = 0.0f;
    float trajectoryFinalPosition_ = 0.0f;
    float trajectoryInitialVelocity_ = 0.0f;
    float trajectoryReachedVelocity_ = 0.0f;
    float trajectoryAcceleration_ = 0.0f;
    float trajectoryDeceleration_ = 0.0f;
    float trajectoryAccelEndPosition_ = 0.0f;
};


#endif
