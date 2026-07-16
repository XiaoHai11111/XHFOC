//
// Created by 24302 on 2025/4/28.
//

#include "cmd_ctrl_motor.h"
#include "motor.h"

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

    focMotor_.SetEnable(true);
    state = RUNNING;
    return true;
}


bool CmdCtrlMotor::Stop()
{
    focMotor_.SetEnable(false);
    state = STOP;
    return true;
}


void CmdCtrlMotor::SetReady(bool _ready)
{
    ready_ = _ready;
    if (!ready_)
    {
        (void)Stop();
    }
}
