#include "pid.h"


float PidController::operator()(float error)
{
    const uint64_t time = micros();
    float dt = (float) (time - timeStamp) * 1e-6f;
    // Quick fix for strange cases (micros overflow)
    if (dt <= 0 || dt > 0.5f) dt = 1e-3f;

    float pTerm = p * error;
    const float integralStep = i * dt * 0.5f * (error + errorLast);
    float iTerm = integralLast + integralStep;
    iTerm = CONSTRAINT(iTerm, -limit, limit);
    float dTerm = d * (error - errorLast) / dt;

    float outputBeforeLimit = pTerm + iTerm + dTerm;

    // Conditional integration anti-windup: when the actuator is already
    // saturated, do not accumulate more integral in the same direction. This
    // lets the velocity loop brake immediately after a long position move.
    if ((outputBeforeLimit > limit && integralStep > 0.0f) ||
        (outputBeforeLimit < -limit && integralStep < 0.0f))
    {
        iTerm = integralLast;
        outputBeforeLimit = pTerm + iTerm + dTerm;
    }

    float output = CONSTRAINT(outputBeforeLimit, -limit, limit);

    // If output ramp defined
    if (outputRamp > 0)
    {
        // Limit the acceleration by ramping the output
        float outputRate = (output - outputLast) / dt;
        if (outputRate > outputRamp)
            output = outputLast + outputRamp * dt;
        else if (outputRate < -outputRamp)
            output = outputLast - outputRamp * dt;
    }

    integralLast = iTerm;
    outputLast = output;
    errorLast = error;
    timeStamp = time;

    return output;
}

void PidController::Reset()
{
    errorLast = 0.0f;
    outputLast = 0.0f;
    integralLast = 0.0f;
    timeStamp = micros();
}
