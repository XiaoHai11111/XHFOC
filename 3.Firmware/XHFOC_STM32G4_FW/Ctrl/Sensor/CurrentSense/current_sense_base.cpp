#include "current_sense_base.h"

DqCurrent_t CurrentSenseBase::GetFocCurrents(float angleElectrical)
{
    const PhaseCurrent_t current = GetPhaseCurrents();

    iAlpha = (current.a - (current.b + current.c) * 0.5f) * 2.0f / 3.0f;
    iBeta = (current.b - current.c) * _SQRT3 / 2 * 2.0f / 3.0f;

    const float ct = CosApprox(angleElectrical);
    const float st = SinApprox(angleElectrical);

    DqCurrent_t focCurrent{};
    focCurrent.d = iAlpha * ct + iBeta * st;
    focCurrent.q = iBeta * ct - iAlpha * st;

    return focCurrent;
}
