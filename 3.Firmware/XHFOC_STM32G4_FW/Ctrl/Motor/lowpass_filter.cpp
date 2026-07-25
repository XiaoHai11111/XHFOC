#include "lowpass_filter.h"


float LowPassFilter::operator()(float _input)
{
    const uint64_t time = micros();
    // Subtract integer timestamps before converting to float. Converting each
    // absolute microsecond timestamp first loses resolution as uptime grows and
    // makes the filter update in visible, irregular steps.
    float dt = static_cast<float>(time - timeStamp) * 1e-6f;

    if (dt <= 0.0f) dt = 1e-3f;
    else if (dt > 0.3f)
    {
        outputLast = _input;
        timeStamp = time;
        return _input;
    }

    float alpha = timeConstant / (timeConstant + dt);
    float output = alpha * outputLast + (1.0f - alpha) * _input;
    outputLast = output;
    timeStamp = time;

    return output;
}
