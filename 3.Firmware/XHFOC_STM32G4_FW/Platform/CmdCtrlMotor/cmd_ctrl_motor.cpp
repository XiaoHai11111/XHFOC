//
// Created by 24302 on 2025/4/28.
//

#include "cmd_ctrl_motor.h"
#include "common_inc.h"
#include "motor.h"
#include <cmath>

namespace
{
constexpr float kMaxAbsolutePositionRad = 1000000.0f;
}

CmdCtrlMotor::CmdCtrlMotor(Motor& _focMotor, uint8_t _id, bool _inverse, uint8_t _reduction,
                           float _angleLimitMin, float _angleLimitMax) :
    nodeID(_id),
    angleLimitMax(_angleLimitMax),
    angleLimitMin(_angleLimitMin),
    inverseDirection(_inverse),
    reduction(_reduction),
    focMotor_(_focMotor)
{
}


bool CmdCtrlMotor::Start()
{
    if (!ready_)
    {
        return false;
    }

    taskENTER_CRITICAL();
    // The rotor may move while the power stage is disabled between FOC
    // initialization and START. If the host has not explicitly supplied a
    // position target, hold the actual position at the instant of enabling
    // instead of chasing the stale initialization-time angle.
    if (focMotor_.config.controlMode == Motor::ANGLE && !positionTargetExplicit_)
    {
        focMotor_.target = focMotor_.GetLastEstimateAngle();
    }
    focMotor_.SetEnable(true);
    state = RUNNING;
    taskEXIT_CRITICAL();
    return true;
}


bool CmdCtrlMotor::Stop()
{
    focMotor_.SetEnable(false);
    state = STOP;
    return true;
}

bool CmdCtrlMotor::SetCurrent(float targetCurrent)
{
    return SetControlTarget(Motor::TORQUE, targetCurrent);
}

bool CmdCtrlMotor::SetVelocity(float targetVelocity)
{
    return SetControlTarget(Motor::VELOCITY, targetVelocity);
}

bool CmdCtrlMotor::SetPosition(float targetPosition)
{
    return SetControlTarget(Motor::ANGLE, targetPosition);
}

bool CmdCtrlMotor::SetControlTarget(Motor::ControlMode_t controlMode, float target)
{
    if (!std::isfinite(target))
    {
        return false;
    }

    float limit = 0.0f;
    switch (controlMode)
    {
        case Motor::TORQUE:
            limit = std::fabs(focMotor_.config.currentLimit);
            break;
        case Motor::VELOCITY:
            limit = std::fabs(focMotor_.config.velocityLimit);
            break;
        case Motor::ANGLE:
            limit = kMaxAbsolutePositionRad;
            break;
        default:
            return false;
    }
    if (std::fabs(target) > limit)
    {
        return false;
    }

    taskENTER_CRITICAL();
    if (controlMode == Motor::ANGLE)
    {
        positionTargetExplicit_ = true;
    }

    // Updating the target of the active mode must remain continuous. Stopping
    // and resetting every time a host streams position commands creates a
    // visible torque gap and mechanical stutter.
    if (focMotor_.config.controlMode == controlMode)
    {
        focMotor_.target = target;
        taskEXIT_CRITICAL();
        return true;
    }

    const bool wasRunning = (state == RUNNING);

    // A real mode change still needs an atomic stop/reset/update/restart so a
    // target expressed in one unit is never consumed by another control mode.
    (void)Stop();
    focMotor_.config.controlMode = controlMode;
    focMotor_.target = target;
    const bool restartOk = !wasRunning || Start();
    taskEXIT_CRITICAL();

    return restartOk;
}


void CmdCtrlMotor::SetReady(bool _ready)
{
    ready_ = _ready;
    if (!ready_)
    {
        (void)Stop();
    }
}
