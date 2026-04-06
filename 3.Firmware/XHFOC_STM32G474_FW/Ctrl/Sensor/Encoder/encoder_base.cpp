#include "encoder_base.h"

#include <cmath>

#include "math_utils.h"
#include "time_utils.h"

void EncoderBase::Update()
{
    const float angle = GetRawAngle();
    angleTimestamp = micros();

    const float deltaAngle = angle - angleLast;
    // Track overflow as full rotation count.
    if (std::fabs(deltaAngle) > (0.8f * _2PI))
    {
        rotationCount += (deltaAngle > 0.0f) ? -1 : 1;
    }

    angleLast = angle;
}

float EncoderBase::GetVelocity()
{
    float time = static_cast<float>(angleTimestamp - velocityTimestamp) * 1e-6f;
    // Quick fix for strange cases (micros overflow)
    if (time <= 0.0f)
    {
        time = 1e-3f;
    }

    const float vel = (static_cast<float>(rotationCount - rotationCountLast) * _2PI + (angleLast - velocityLast)) / time;

    velocityLast = angleLast;
    rotationCountLast = rotationCount;
    velocityTimestamp = angleTimestamp;

    return vel;
}

void EncoderBase::VarInit()
{
    // Ensure smooth startup values for velocity and angle filters.
    (void)GetRawAngle();
    delayMicroSeconds(1);

    velocityLast = GetRawAngle();
    velocityTimestamp = micros();
    delay(1);

    (void)GetRawAngle();
    delayMicroSeconds(1);

    angleLast = GetRawAngle();
    angleTimestamp = micros();
}

float EncoderBase::GetLapAngle()
{
    return angleLast;
}

float EncoderBase::GetFullAngle()
{
    return static_cast<float>(rotationCount) * _2PI + angleLast;
}

int32_t EncoderBase::GetRotationCount()
{
    return rotationCount;
}
