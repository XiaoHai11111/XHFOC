#include "current_sense_base.h"

float CurrentSenseBase::GetDcCurrent(float angleElectrical)
{
    const PhaseCurrent_t current = GetPhaseCurrents();

    float a = 0.0f;
    float b = 0.0f;

    if (pwmDutyA > dropOnePhaseDutyThreshold ||
        pwmDutyB > dropOnePhaseDutyThreshold ||
        pwmDutyC > dropOnePhaseDutyThreshold)
    {
        switch (sector)
        {
            case 1:
            case 6:
                a = -current.b - current.c;
                b = current.b;
                break;
            case 2:
            case 3:
                a = current.a;
                b = -current.a - current.c;
                break;
            case 4:
            case 5:
                a = current.a;
                b = current.b;
                break;
            default:
                a = current.a;
                b = current.b;
                break;
        }
    }
    else
    {
        const float mid = (1.0f / 3.0f) * (current.a + current.b + current.c);
        a = current.a - mid;
        b = current.b - mid;
    }

    iAlpha = a;
    iBeta = _1_SQRT3 * a + _2_SQRT3 * b;

    const float sign = (iBeta * CosApprox(angleElectrical) - iAlpha * SinApprox(angleElectrical)) > 0.0f ? 1.0f : -1.0f;
    return sign * SQRT(iAlpha * iAlpha + iBeta * iBeta);
}

DqCurrent_t CurrentSenseBase::GetFocCurrents(float angleElectrical)
{
    const PhaseCurrent_t current = GetPhaseCurrents();

    float a = 0.0f;
    float b = 0.0f;

    if (pwmDutyA > dropOnePhaseDutyThreshold ||
        pwmDutyB > dropOnePhaseDutyThreshold ||
        pwmDutyC > dropOnePhaseDutyThreshold)
    {
        switch (sector)
        {
            case 1:
            case 6:
                a = -current.b - current.c;
                b = current.b;
                break;
            case 2:
            case 3:
                a = current.a;
                b = -current.a - current.c;
                break;
            case 4:
            case 5:
                a = current.a;
                b = current.b;
                break;
            default:
                a = current.a;
                b = current.b;
                break;
        }
    }
    else
    {
        const float mid = (1.0f / 3.0f) * (current.a + current.b + current.c);
        a = current.a - mid;
        b = current.b - mid;
    }

    iAlpha = a;
    iBeta = _1_SQRT3 * a + _2_SQRT3 * b;

    const float ct = CosApprox(angleElectrical);
    const float st = SinApprox(angleElectrical);

    DqCurrent_t focCurrent{};
    focCurrent.d = iAlpha * ct + iBeta * st;
    focCurrent.q = iBeta * ct - iAlpha * st;

    return focCurrent;
}
