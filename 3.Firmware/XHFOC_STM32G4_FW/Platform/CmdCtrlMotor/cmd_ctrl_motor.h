//
// Created by 24302 on 2025/4/28.
//

#ifndef CMDCTRLMOTOR_H
#define CMDCTRLMOTOR_H

#include "motor.h"
#include "protocol.hpp"

class CmdCtrlMotor {

public:
    enum State
    {
        RUNNING,
        FINISH,
        STOP
    };


    const uint32_t CTRL_CIRCLE_COUNT = 200 * 256;

    CmdCtrlMotor(Motor& _focMotor, uint8_t _id, bool _inverse = false, uint8_t _reduction = 1,
                  float _angleLimitMin = -180, float _angleLimitMax = 180);

    bool Start();
    bool Stop();
    bool SetCurrent(float targetCurrent);
    bool SetVelocity(float targetVelocity);
    bool SetPosition(float targetPosition);
    void SetReady(bool _ready);

    uint8_t nodeID;
    float angle = 0;
    float angleLimitMax;
    float angleLimitMin;
    bool inverseDirection;
    uint8_t reduction;
    State state = STOP;

    // Communication protocol definitions
    auto MakeProtocolDefinitions()
    {
        return make_protocol_member_list(
            make_protocol_ro_property("angle", &angle),
            make_protocol_function("start", *this, &CmdCtrlMotor::Start),
            make_protocol_function("stop", *this, &CmdCtrlMotor::Stop),
            make_protocol_function("set_current", *this, &CmdCtrlMotor::SetCurrent, "target"),
            make_protocol_function("set_velocity", *this, &CmdCtrlMotor::SetVelocity, "target"),
            make_protocol_function("set_position", *this, &CmdCtrlMotor::SetPosition, "target")
        );
    }

private:
    bool SetControlTarget(Motor::ControlMode_t controlMode, float target);

    Motor& focMotor_;
    volatile bool ready_ = false;
    bool positionTargetExplicit_ = false;
};

#endif //CMDCTRLMOTOR_H
