#ifndef FlashStorage_STM32_h
#define FlashStorage_STM32_h

#include <cstdint>
#include "random_flash_utils.h"


class EEPROM
{
public:
    EEPROM()
    = default;


    uint8_t Read(int _address)
    {
        if (!isInitialized)
            init();

        if (_address < 0 || static_cast<uint32_t>(_address) >= EEPROM_SIZE)
        {
            isValid = false;
            return 0xFFU;
        }

        return EEPROMReadBufferedByte(_address);
    }


    void Update(int _address, uint8_t _value)
    {
        if (!isInitialized)
            init();

        if (_address < 0 || static_cast<uint32_t>(_address) >= EEPROM_SIZE)
        {
            isValid = false;
            return;
        }

        if (EEPROMReadBufferedByte(_address) != _value)
        {
            EEPROMWriteBufferedByte(_address, _value);
            if (commitASAP)
            {
                isValid = EEPROMBufferFlush();
                dirtyBuffer = !isValid;
            }
            else
            {
                dirtyBuffer = true;
            }
        }
    }


    void Write(int _address, uint8_t _value)
    {
        Update(_address, _value);
    }


    template<typename T>
    T &Pull(int _offset, T &_t)
    {
        // Copy the data from the flash to the buffer if not yet
        if (!isInitialized)
            init();

        if (_offset < 0 || static_cast<uint32_t>(_offset) + sizeof(T) > EEPROM_SIZE)
        {
            isValid = false;
            return _t;
        }

        uint16_t offset = static_cast<uint16_t>(_offset);
        auto* _pointer = (uint8_t*) &_t;

        for (uint16_t count = sizeof(T); count; --count, ++offset)
        {
            *_pointer++ = EEPROMReadBufferedByte(offset);
        }

        return _t;
    }


    template<typename T>
    const T &Push(int _idx, const T &_t)
    {
        // Copy the data from the flash to the buffer if not yet
        if (!isInitialized) init();

        if (_idx < 0 || static_cast<uint32_t>(_idx) + sizeof(T) > EEPROM_SIZE)
        {
            isValid = false;
            return _t;
        }

        uint16_t offset = static_cast<uint16_t>(_idx);

        const auto* _pointer = (const uint8_t*) &_t;

        bool changed = false;
        for (uint16_t count = sizeof(T); count; --count, ++offset)
        {
            const uint8_t value = *_pointer++;
            if (EEPROMReadBufferedByte(offset) != value)
            {
                EEPROMWriteBufferedByte(offset, value);
                changed = true;
            }
        }

        if (!changed)
        {
            return _t;
        }

        if (commitASAP)
        {
            // Save the data from the buffer to the flash right away
            isValid = EEPROMBufferFlush();
            dirtyBuffer = !isValid;
        } else
        {
            // Delay saving the data from the buffer to the flash. Just flag and wait for commit() later
            dirtyBuffer = true;
        }

        return _t;
    }


    bool Commit()
    {
        if (!isInitialized)
            init();

        if (dirtyBuffer)
        {
            // Save the data from the buffer to the flash
            isValid = EEPROMBufferFlush();
            dirtyBuffer = !isValid;
        }

        return isValid;
    }


    static uint16_t TotalSize()
    {
        return EEPROM_SIZE;
    }


    void SetCommitASAP(bool value = true)
    {
        commitASAP = value;
    }


    bool isValid = true;


private:
    void init()
    {
        // Copy the data from the flash to the buffer
        EEPROMFillBuffer();
        isInitialized = true;
    }


    bool isInitialized = false;
    bool dirtyBuffer = false;
    bool commitASAP = true;
};


#endif
