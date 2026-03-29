#ifndef XHFOC_KEY_STM32_H
#define XHFOC_KEY_STM32_H

#include "key_base.h"

class Key : public KeyBase
{
public:
    explicit Key(uint8_t id,
                 uint32_t debounceMsIn = 20,
                 uint32_t doubleClickGapMsIn = 250,
                 uint32_t longPressMsIn = 800)
        : KeyBase(id, debounceMsIn, doubleClickGapMsIn, longPressMsIn)
    {
    }

protected:
    bool ReadButtonPressed(uint8_t id) override;
};

#endif
