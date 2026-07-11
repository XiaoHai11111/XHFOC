#include "current_sense_base.h"

DqCurrent_t CurrentSenseBase::GetFocCurrents(float angleElectrical)
{
    const PhaseCurrent_t current = GetPhaseCurrents();
    lastPhaseCurrent_ = current;

    alphaBetaCurrent_.iAlpha = (current.a - (current.b + current.c) * 0.5f) * 2.0f / 3.0f;
    alphaBetaCurrent_.iBeta = (current.b - current.c) * _SQRT3 / 2 * 2.0f / 3.0f;

    const float ct = CosApprox(angleElectrical);
    const float st = SinApprox(angleElectrical);

    DqCurrent_t focCurrent{};
    focCurrent.d = alphaBetaCurrent_.iAlpha * ct + alphaBetaCurrent_.iBeta * st;
    focCurrent.q = alphaBetaCurrent_.iBeta * ct - alphaBetaCurrent_.iAlpha * st;
    lastDqCurrent_ = focCurrent;

    return focCurrent;
}

PhaseCurrent_t CurrentSenseBase::GetLastPhaseCurrents() const
{
    return lastPhaseCurrent_;
}

DqCurrent_t CurrentSenseBase::GetLastDqCurrents() const
{
    return lastDqCurrent_;
}

AlphaBetaCurrent_t CurrentSenseBase::GetLastAlphaBetaCurrents() const
{
    return alphaBetaCurrent_;
}
