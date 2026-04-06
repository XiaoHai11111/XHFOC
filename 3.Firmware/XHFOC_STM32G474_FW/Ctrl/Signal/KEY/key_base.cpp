#include "key_base.h"

void KeyBase::Init()
{
    rawPressedLast_ = ReadButtonPressed(id_);
    stablePressed_ = rawPressedLast_;
    initialized_ = true;
    debounceAccumMs_ = 0;
    pressDurationMs_ = 0;
    clickWaitMs_ = 0;
    longPressReported_ = false;
    waitSecondClick_ = false;
}

void KeyBase::Emit(Event event) const
{
    if (onEvent_ != nullptr)
    {
        onEvent_(id_, event);
    }
}

void KeyBase::Tick(uint32_t timeElapseMs)
{
    if (!initialized_)
    {
        Init();
    }

    const bool rawPressed = ReadButtonPressed(id_);

    if (rawPressed != rawPressedLast_)
    {
        rawPressedLast_ = rawPressed;
        debounceAccumMs_ = 0;
    }
    else if (debounceAccumMs_ < debounceMs)
    {
        debounceAccumMs_ += timeElapseMs;
        if (debounceAccumMs_ >= debounceMs && stablePressed_ != rawPressed)
        {
            stablePressed_ = rawPressed;
            if (stablePressed_)
            {
                pressDurationMs_ = 0;
                longPressReported_ = false;
            }
            else
            {
                if (!longPressReported_)
                {
                    if (waitSecondClick_)
                    {
                        waitSecondClick_ = false;
                        clickWaitMs_ = 0;
                        Emit(EVENT_DOUBLE_CLICK);
                    }
                    else
                    {
                        waitSecondClick_ = true;
                        clickWaitMs_ = 0;
                    }
                }
                longPressReported_ = false;
            }
        }
    }

    if (stablePressed_)
    {
        pressDurationMs_ += timeElapseMs;
        if (!longPressReported_ && pressDurationMs_ >= longPressMs)
        {
            longPressReported_ = true;
            waitSecondClick_ = false;
            clickWaitMs_ = 0;
            Emit(EVENT_LONG_PRESS);
        }
    }
    else if (waitSecondClick_)
    {
        clickWaitMs_ += timeElapseMs;
        if (clickWaitMs_ >= doubleClickGapMs)
        {
            waitSecondClick_ = false;
            clickWaitMs_ = 0;
            Emit(EVENT_SINGLE_CLICK);
        }
    }
}
