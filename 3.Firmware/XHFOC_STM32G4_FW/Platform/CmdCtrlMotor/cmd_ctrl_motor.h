//
// Created by 24302 on 2025/4/28.
//

#ifndef CMDCTRLMOTOR_H
#define CMDCTRLMOTOR_H

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

    CmdCtrlMotor(uint8_t _id, bool _inverse = false, uint8_t _reduction = 1,
                  float _angleLimitMin = -180, float _angleLimitMax = 180);

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
            make_protocol_ro_property("angle", &angle)
        );
    }

};

#endif //CMDCTRLMOTOR_H
