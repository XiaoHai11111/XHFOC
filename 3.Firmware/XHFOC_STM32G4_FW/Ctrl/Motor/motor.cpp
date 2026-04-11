#include "motor.h"

#include "main.h"
#include "time_utils.h"
#include <cmath>

bool Motor::Init(float _zeroElectricOffset, EncoderBase::Direction _encoderDir)
{
    if (encoder)
    {
        (void)encoder->Init();
    }
    if (!driver->Init())
    {
        return false;
    }
    if (currentSense) currentSense->Init();

    if (!currentSense && ASSERT(phaseResistance))
    {
        float limit = config.currentLimit * phaseResistance;
        config.voltageLimit = limit < config.voltageLimit ? limit : config.voltageLimit;
    }

    if (config.voltageLimit > driver->voltagePowerSupply)
        config.voltageLimit = driver->voltagePowerSupply;

    if (config.voltageUsedForSensorAlign > config.voltageLimit)
        config.voltageUsedForSensorAlign = config.voltageLimit;

    if (currentSense)
    {
        config.pidCurrentQ.limit = config.voltageLimit;
        config.pidCurrentD.limit = config.voltageLimit;
        config.pidVelocity.limit = config.currentLimit;
    } else if (ASSERT(phaseResistance))
    {
        config.pidVelocity.limit = config.currentLimit;
    } else
    {
        config.pidVelocity.limit = config.voltageLimit;
    }
    config.pidAngle.limit = config.velocityLimit;

    return InitFOC(_zeroElectricOffset, _encoderDir);
}


void Motor::AttachEncoder(EncoderBase* _encoder)
{
    encoder = _encoder;
}


void Motor::AttachCurrentSense(CurrentSenseBase* _currentSense)
{
    currentSense = _currentSense;
}


void Motor::AttachDriver(DriverBase* _driver)
{
    driver = _driver;
}


void Motor::SetEnable(bool _enable)
{
    if (enabled == _enable)
    {
        return;
    }

    enabled = _enable;
    if (!enabled)
    {
        setPointCurrent = 0.0f;
        setPointCurrentTarget = 0.0f;
        setPointVelocity = 0.0f;
        setPointAngle = 0.0f;
        config.pidCurrentQ.Reset();
        config.pidCurrentD.Reset();
        config.pidVelocity.Reset();
        config.pidAngle.Reset();
    }
    // if (driver)
    // {
    //     driver->SetEnable(_enable);
    // }
}


float Motor::GetEstimateAngle()
{
    // If no sensor linked return previous value (for open-loop case)
    if (!encoder) return estimateAngle;
    state.rawAngle = (float) (encoder->countDirection) * encoder->GetFullAngle();
    state.estAngle = config.lpfAngle(state.rawAngle);

    return state.estAngle;
}


float Motor::GetEstimateVelocity()
{
    // If no sensor linked return previous value (for open-loop case)
    if (!encoder) return estimateVelocity;
    state.rawVelocity = (float) (encoder->countDirection) * encoder->GetVelocity();
    state.estVelocity = config.lpfVelocity(state.rawVelocity);

    return state.estVelocity;
}


float Motor::GetElectricalAngle()
{
    // If no sensor linked return previous value (for open-loop case)
    return encoder ? Normalize((float) (encoder->countDirection * polePairs)
                               * encoder->GetLapAngle() - zeroElectricAngleOffset)
                   : electricalAngle;
}


void Motor::Tick()
{
    CloseLoopControlTick();
    FocOutputTick();
}

void Motor::SetControlLoopHz(float _hz)
{
    if (_hz > 1.0f)
    {
        controlLoopDeltaT_ = 1.0f / _hz;
        const float velocityLoopHz = 1000.0f;
        uint16_t decimation = (uint16_t)std::round(_hz / velocityLoopHz);
        if (decimation < 1U) decimation = 1U;
        velocityLoopDecimation_ = decimation;
        velocityLoopCounter_ = 0U;
    }
    else
    {
        controlLoopDeltaT_ = 0.0f;
        velocityLoopDecimation_ = 1U;
        velocityLoopCounter_ = 0U;
    }
}


bool Motor::InitFOC(float _zeroElectricOffset, EncoderBase::Direction _sensorDirection)
{
    // Absolute zero offset provided, no need to align
    if (ASSERT(_zeroElectricOffset) && encoder)
    {
        zeroElectricAngleOffset = _zeroElectricOffset;
        encoder->countDirection = _sensorDirection;
    }

    if (encoder)
    {
        if (!AlignSensor()) return false;

        encoder->Update();
        estimateAngle = GetEstimateAngle();
    }

    return true;
}


bool Motor::AlignSensor()
{
    if ((encoder == nullptr) || (driver == nullptr))
    {
        return false;
    }

    if (encoder->countDirection == EncoderBase::UNKNOWN)
    {
        driver->SetEnable(true);
        HAL_Delay(100);

        // Find natural direction
        for (int i = 0; i <= 500; i++)
        {
            float angle = _3PI_2 + _2PI * (float) i / 500.0f;
            SetPhaseVoltage(config.voltageUsedForSensorAlign, 0, angle);
            HAL_Delay(2);
        }

        encoder->Update();
        float midAngle = encoder->GetFullAngle();

        for (int i = 500; i >= 0; i--)
        {
            float angle = _3PI_2 + _2PI * (float) i / 500.0f;
            SetPhaseVoltage(config.voltageUsedForSensorAlign, 0, angle);
            HAL_Delay(2);
        }

        encoder->Update();
        float endAngle = encoder->GetFullAngle();

        SetPhaseVoltage(0, 0, 0);
        driver->SetEnable(false);
        HAL_Delay(200);

        // Determine the direction the sensor moved
        if (midAngle == endAngle)
        {
            error = FAILED_TO_NOTICE_MOVEMENT;
            return false;
        } else if (midAngle < endAngle)
        {
            encoder->countDirection = EncoderBase::Direction::CCW;
        } else
        {
            encoder->countDirection = EncoderBase::Direction::CW;
        }

        // Check pole pair number
        float deltaAngle = std::fabs(midAngle - endAngle);
        if (std::fabs(deltaAngle * (float) polePairs - _2PI) > 0.5f)
        {
            // 0.5f is arbitrary number, it can be tuned
            error = POLE_PAIR_MISMATCH;
            return false;
        }
    }

    // Align the electrical phases of the motor and sensor
    if (!ASSERT(zeroElectricAngleOffset))
    {
        driver->SetEnable(true);
        // Set angle -90(270 = 3PI/2) degrees
        SetPhaseVoltage(config.voltageUsedForSensorAlign, 0, _3PI_2);

        HAL_Delay(1000);
        encoder->Update();
        zeroElectricAngleOffset = 0; // Clear offset first
        zeroElectricAngleOffset = GetElectricalAngle();

        SetPhaseVoltage(0, 0, 0);
        driver->SetEnable(false);
        HAL_Delay(200);
    }

    return true;
}


void Motor::CloseLoopControlTick()
{
    // No need to read shaftAngle in open-loop mode
    if (config.controlMode != ControlMode_t::ANGLE_OPEN_LOOP &&
        config.controlMode != ControlMode_t::VELOCITY_OPEN_LOOP)
    {
        estimateAngle = GetEstimateAngle();
    }

    estimateVelocity = GetEstimateVelocity();

    if (!enabled) return;
    if (velocityLoopCounter_ + 1U >= velocityLoopDecimation_)
    {
        velocityLoopCounter_ = 0U;
    }
    else
    {
        velocityLoopCounter_++;
    }
    const bool runVelocityLoop = (velocityLoopCounter_ == 0U);

    switch (config.controlMode)
    {
        case ControlMode_t::TORQUE:
            setPointCurrentTarget = target;
            break;
        case ControlMode_t::ANGLE:
            setPointAngle = target;
            if (runVelocityLoop)
            {
                setPointVelocity = config.pidAngle(setPointAngle - estimateAngle);
                setPointCurrentTarget = config.pidVelocity(setPointVelocity - estimateVelocity);
            }
            break;
        case ControlMode_t::VELOCITY:
            if (runVelocityLoop)
            {
                setPointVelocity = target;
                setPointCurrentTarget = config.pidVelocity(setPointVelocity - estimateVelocity);
            }
            break;
        case ControlMode_t::VELOCITY_OPEN_LOOP:
            setPointVelocity = target;
            voltage.q = VelocityOpenLoopTick(setPointVelocity);
            voltage.d = 0;
            break;
        case ControlMode_t::ANGLE_OPEN_LOOP:
            setPointAngle = target;
            voltage.q = AngleOpenLoopTick(setPointAngle);
            voltage.d = 0;
            break;
    }

    if (config.controlMode == ControlMode_t::TORQUE ||
        config.controlMode == ControlMode_t::VELOCITY ||
        config.controlMode == ControlMode_t::ANGLE)
    {
        float maxCurrent = config.currentLimit;
        if (maxCurrent < 0.0f) maxCurrent = -maxCurrent;
        if (setPointCurrentTarget > maxCurrent) setPointCurrentTarget = maxCurrent;
        if (setPointCurrentTarget < -maxCurrent) setPointCurrentTarget = -maxCurrent;

        float dt = controlLoopDeltaT_;
        if (dt <= 0.0f || dt > 0.05f) dt = 0.0002f;
        float stepMax = currentSetpointRamp_ * dt;
        float delta = setPointCurrentTarget - setPointCurrent;
        if (delta > stepMax) delta = stepMax;
        if (delta < -stepMax) delta = -stepMax;
        setPointCurrent += delta;
    }
}


void Motor::FocOutputTick()
{
    if (encoder) encoder->Update();
    if (driver == nullptr) return;

    if (config.controlMode == ControlMode_t::ANGLE_OPEN_LOOP ||
        config.controlMode == ControlMode_t::VELOCITY_OPEN_LOOP)
        return;

    if (!enabled) return;

    electricalAngle = GetElectricalAngle();

    if (currentSense)
    {
        current = currentSense->GetFocCurrents(electricalAngle);
        current.d = config.lpfCurrentD(current.d);
        current.q = config.lpfCurrentQ(current.q);

        voltage.d = config.pidCurrentD(-current.d);
        voltage.q = config.pidCurrentQ(setPointCurrent - current.q);
    }
    else
    {
        voltage.q = ASSERT(phaseResistance) ? setPointCurrent * phaseResistance : setPointCurrent;
        voltage.d = 0;
    }

    const float voltageMagnitude = SQRT(voltage.d * voltage.d + voltage.q * voltage.q);
    if (voltageMagnitude > config.voltageLimit && voltageMagnitude > 1e-6f)
    {
        const float scale = config.voltageLimit / voltageMagnitude;
        voltage.d *= scale;
        voltage.q *= scale;
    }

    SetPhaseVoltage(voltage.q, voltage.d, electricalAngle);
}


float Motor::VelocityOpenLoopTick(float _target)
{
    float deltaT = controlLoopDeltaT_;
    uint64_t t = openLoopTimestamp;
    if (deltaT <= 0.0f)
    {
        t = micros();
        deltaT = (float) (t - openLoopTimestamp) * 1e-6f;
        // Quick fix for strange cases (micros overflow or timestamp not defined)
        if (deltaT <= 0 || deltaT > 0.5f) deltaT = 1e-3f;
    }

    estimateAngle = Normalize(estimateAngle + _target * deltaT);
    estimateVelocity = _target;

    const float absTarget = std::fabs(_target);
    if (absTarget < 0.2f)
    {
        SetPhaseVoltage(0, 0, Normalize(estimateAngle) * (float) polePairs);
        openLoopTimestamp = t;
        return 0.0f;
    }

    const float baseVoltageQ = ASSERT(phaseResistance) ? config.currentLimit * phaseResistance : config.voltageLimit;
    float scale = absTarget / (config.velocityLimit > 1e-3f ? config.velocityLimit : 1.0f);
    if (scale < 0.15f) scale = 0.15f;
    if (scale > 1.0f) scale = 1.0f;
    float voltageQ = baseVoltageQ * scale;
    SetPhaseVoltage(voltageQ, 0, Normalize(estimateAngle) * (float) polePairs);

    openLoopTimestamp = t;

    return voltageQ;


    // auto t = micros();
    // float deltaT = (float) (t - openLoopTimestamp) * 1e-6f;
    // // Quick fix for strange cases (micros overflow or timestamp not defined)
    // if (deltaT <= 0 || deltaT > 0.5f) deltaT = 1e-3f;
    //
    // estimateAngle = Normalize(estimateAngle + _target * deltaT);
    // estimateVelocity = _target;
    //
    // float voltageQ = ASSERT(phaseResistance) ? config.currentLimit * phaseResistance : config.voltageLimit;
    // SetPhaseVoltage(voltageQ, 0, Normalize(estimateAngle) * (float) polePairs);
    //
    // openLoopTimestamp = t;
    //
    // return voltageQ;


}


float Motor::AngleOpenLoopTick(float _target)
{
    float deltaT = controlLoopDeltaT_;
    uint64_t t = openLoopTimestamp;
    if (deltaT <= 0.0f)
    {
        t = micros();
        deltaT = (float) (t - openLoopTimestamp) * 1e-6f;
        // Quick fix for strange cases (micros overflow or timestamp not defined)
        if (deltaT <= 0 || deltaT > 0.5f) deltaT = 1e-3f;
    }

    if (std::abs(_target - estimateAngle) > std::abs(config.velocityLimit * deltaT))
    {
        estimateAngle += SIGN(_target - estimateAngle) * std::abs(config.velocityLimit) * deltaT;
        estimateVelocity = config.velocityLimit;
    } else
    {
        estimateAngle = _target;
        estimateVelocity = 0;
    }

    if (std::fabs(estimateVelocity) < 0.2f)
    {
        SetPhaseVoltage(0, 0, Normalize(estimateAngle) * (float) polePairs);
        openLoopTimestamp = t;
        return 0.0f;
    }

    float voltageQ = ASSERT(phaseResistance) ? config.currentLimit * phaseResistance : config.voltageLimit;
    SetPhaseVoltage(voltageQ, 0, Normalize(estimateAngle) * (float) polePairs);

    openLoopTimestamp = t;

    return voltageQ;

    // unsigned long t = micros();
    // float deltaT = (float) (t - openLoopTimestamp) * 1e-6f;
    // // Quick fix for strange cases (micros overflow or timestamp not defined)
    // if (deltaT <= 0 || deltaT > 0.5f) deltaT = 1e-3f;
    //
    // if (std::abs(_target - estimateAngle) > std::abs(config.velocityLimit * deltaT))
    // {
    //     estimateAngle += SIGN(_target - estimateAngle) * std::abs(config.velocityLimit) * deltaT;
    //     estimateVelocity = config.velocityLimit;
    // } else
    // {
    //     estimateAngle = _target;
    //     estimateVelocity = 0;
    // }
    //
    // float voltageQ = ASSERT(phaseResistance) ? config.currentLimit * phaseResistance : config.voltageLimit;
    // SetPhaseVoltage(voltageQ, 0, Normalize(estimateAngle) * (float) polePairs);
    //
    // openLoopTimestamp = t;
    //
    // return voltageQ;

}


void Motor::SetPhaseVoltage(float _voltageQ, float _voltageD, float _angleElectrical)
{
    if (driver == nullptr) return;

    float uOut;

    if (_voltageD != 0)
    {
        uOut = SQRT(_voltageD * _voltageD + _voltageQ * _voltageQ) / driver->voltagePowerSupply;
        _angleElectrical = Normalize(_angleElectrical + std::atan2(_voltageQ, _voltageD));
    } else
    {
        uOut = _voltageQ / driver->voltagePowerSupply;
        _angleElectrical = Normalize(_angleElectrical + _PI_2);
    }
    if (uOut < 0.0f)
    {
        uOut = -uOut;
        _angleElectrical = Normalize(_angleElectrical + _PI);
    }
    // SVPWM linear modulation range: keep margin to avoid overmodulation jitter at high speed.
    constexpr float kMaxUOutLinear = 0.577f;
    if (uOut > kMaxUOutLinear)
    {
        uOut = kMaxUOutLinear;
    }
    uint8_t sec = (int) (std::floor(_angleElectrical / _PI_3)) + 1;
    float t1 = _SQRT3
               * SinApprox((float) (sec) * _PI_3 - _angleElectrical)
               * uOut;
    float t2 = _SQRT3
               * SinApprox(_angleElectrical - ((float) (sec) - 1.0f) * _PI_3)
               * uOut;
    float t0 = 1 - t1 - t2;

    float tA, tB, tC;
    switch (sec)
    {
        case 1:
            tA = t1 + t2 + t0 / 2;
            tB = t2 + t0 / 2;
            tC = t0 / 2;
            break;
        case 2:
            tA = t1 + t0 / 2;
            tB = t1 + t2 + t0 / 2;
            tC = t0 / 2;
            break;
        case 3:
            tA = t0 / 2;
            tB = t1 + t2 + t0 / 2;
            tC = t2 + t0 / 2;
            break;
        case 4:
            tA = t0 / 2;
            tB = t1 + t0 / 2;
            tC = t1 + t2 + t0 / 2;
            break;
        case 5:
            tA = t2 + t0 / 2;
            tB = t0 / 2;
            tC = t1 + t2 + t0 / 2;
            break;
        case 6:
            tA = t1 + t2 + t0 / 2;
            tB = t0 / 2;
            tC = t1 + t0 / 2;
            break;
        default:
            tA = 0;
            tB = 0;
            tC = 0;
    }

    // calculate the phase voltages and center
    voltageA = tA * driver->voltagePowerSupply;
    voltageB = tB * driver->voltagePowerSupply;
    voltageC = tC * driver->voltagePowerSupply;

    driver->SetVoltage(voltageA, voltageB, voltageC);
}


// void Motor::SetTorqueLimit(float _val)
// {
//     if (driver == nullptr)
//     {
//         return;
//     }
//
//     config.voltageLimit = _val;
//     config.currentLimit = _val;
//
//     if (!currentSense && ASSERT(phaseResistance))
//     {
//         float limit = config.currentLimit * phaseResistance;
//         config.voltageLimit = limit < config.voltageLimit ? limit : config.voltageLimit;
//     }
//
//     if (config.voltageLimit > driver->voltagePowerSupply)
//         config.voltageLimit = driver->voltagePowerSupply;
//
//     if (config.voltageUsedForSensorAlign > config.voltageLimit)
//         config.voltageUsedForSensorAlign = config.voltageLimit;
//
//     if (currentSense)
//     {
//         config.pidCurrentQ.limit = config.voltageLimit;
//         config.pidCurrentD.limit = config.voltageLimit;
//         config.pidVelocity.limit = config.currentLimit;
//     } else if (ASSERT(phaseResistance))
//     {
//         config.pidVelocity.limit = config.currentLimit;
//     } else
//     {
//         config.pidVelocity.limit = config.voltageLimit;
//     }
//     config.pidAngle.limit = config.velocityLimit;
// }
