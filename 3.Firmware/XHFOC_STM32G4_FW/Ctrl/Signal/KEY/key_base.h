#ifndef XHFOC_KEY_BASE_H
#define XHFOC_KEY_BASE_H

#include <cstdint>

class KeyBase
{
public:
    enum Event
    {
        EVENT_SINGLE_CLICK = 0,
        EVENT_DOUBLE_CLICK,
        EVENT_LONG_PRESS
    };

    typedef void (*OnEventCallback)(uint8_t keyId, Event event);

    explicit KeyBase(uint8_t id,
                     uint32_t debounceMsIn = 20,
                     uint32_t doubleClickGapMsIn = 250,
                     uint32_t longPressMsIn = 800)
        : id_(id)
    {
        debounceMs = debounceMsIn;
        doubleClickGapMs = doubleClickGapMsIn;
        longPressMs = longPressMsIn;
    }

    virtual ~KeyBase() = default;

    void Init();
    void Tick(uint32_t timeElapseMs);
    void SetOnEventListener(OnEventCallback callback)
    { onEvent_ = callback; }

protected:
    virtual bool ReadButtonPressed(uint8_t id) = 0;

    uint32_t debounceMs = 20;
    uint32_t doubleClickGapMs = 250;
    uint32_t longPressMs = 800;

private:
    void Emit(Event event) const;

    uint8_t id_;
    OnEventCallback onEvent_ = nullptr;

    bool rawPressedLast_ = false;
    bool stablePressed_ = false;
    bool initialized_ = false;

    uint32_t debounceAccumMs_ = 0;
    uint32_t pressDurationMs_ = 0;
    uint32_t clickWaitMs_ = 0;

    bool longPressReported_ = false;
    bool waitSecondClick_ = false;
};

#endif
