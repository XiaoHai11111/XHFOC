//
// Created by 24302 on 2025/4/28.
//

#include "cmd_ctrl_motor.h"
#include "common_inc.h"

CmdCtrlMotor::CmdCtrlMotor(uint8_t _id, bool _inverse,uint8_t _reduction, float _angleLimitMin, float _angleLimitMax) :
    nodeID(_id), inverseDirection(_inverse), reduction(_reduction),angleLimitMin(_angleLimitMin), angleLimitMax(_angleLimitMax)
{

}