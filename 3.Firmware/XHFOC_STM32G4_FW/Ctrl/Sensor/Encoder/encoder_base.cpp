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
    // At a 10 kHz control rate one MT6816 count would appear as roughly
    // 3.8 rad/s when differentiated sample-by-sample. Accumulate encoder
    // movement over a 1 ms window instead, then hold that estimate between
    // updates. This reduces the single-count step to about 0.38 rad/s while
    // retaining enough bandwidth for the outer position loop.
    constexpr uint64_t kVelocityWindowUs = 1000U;
    const uint64_t elapsedUs = angleTimestamp - velocityTimestamp;
    if (elapsedUs < kVelocityWindowUs)
    {
        return velocityEstimate;
    }

    const float elapsedSeconds = static_cast<float>(elapsedUs) * 1e-6f;
    if (elapsedSeconds <= 0.0f || elapsedSeconds > 0.1f)
    {
        velocityEstimate = 0.0f;
    }
    else
    {
        const float deltaPosition =
                static_cast<float>(rotationCount - rotationCountLast) * _2PI +
                (angleLast - velocityLast);
        velocityEstimate = deltaPosition / elapsedSeconds;
    }

    velocityLast = angleLast;
    rotationCountLast = rotationCount;
    velocityTimestamp = angleTimestamp;

    return velocityEstimate;
}

void EncoderBase::VarInit()
{
    // Ensure smooth startup values for velocity and angle filters.
    (void)GetRawAngle();
    delayMicroSeconds(1);

    velocityLast = GetRawAngle();
    velocityTimestamp = micros();
    velocityEstimate = 0.0f;
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
