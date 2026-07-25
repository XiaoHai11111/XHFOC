#include "random_flash_utils.h"

#include <cstring>

namespace {
constexpr uint32_t kFlashBaseAddr = 0x08000000U;
constexpr uint32_t kFlashSizeBytes = 128U * 1024U;  // STM32G431
constexpr uint32_t kFlashTotalPages = kFlashSizeBytes / FLASH_PAGE_SIZE;
constexpr uint32_t kEepromPages = 1U;
constexpr uint32_t kEepromStartPage = kFlashTotalPages - kEepromPages;
constexpr uint32_t kEepromBaseAddress = kFlashBaseAddr + (kEepromStartPage * FLASH_PAGE_SIZE);

uint8_t eepromBuffer[EEPROM_SIZE] __attribute__((aligned(8))) = {0};

static_assert(FLASH_PAGE_SIZE == 2U * 1024U, "Update the reserved Flash layout for this MCU");
static_assert(EEPROM_SIZE <= FLASH_PAGE_SIZE, "EEPROM buffer must fit in one Flash page");
}

uint8_t EEPROMReadByte(uint32_t _pos)
{
    EEPROMFillBuffer();
    return EEPROMReadBufferedByte(_pos);
}

bool EEPROMWriteByte(uint32_t _pos, uint8_t _value)
{
    if (_pos >= EEPROM_SIZE)
    {
        return false;
    }

    EEPROMFillBuffer();
    EEPROMWriteBufferedByte(_pos, _value);
    return EEPROMBufferFlush();
}

void EEPROMFillBuffer()
{
    std::memcpy(eepromBuffer, reinterpret_cast<const void*>(kEepromBaseAddress), EEPROM_SIZE);
}

bool EEPROMBufferFlush()
{
    FLASH_EraseInitTypeDef eraseInitStruct = {0};
    uint32_t pageError = 0U;
    bool success = true;

    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return false;
    }

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

    eraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
    eraseInitStruct.Banks = FLASH_BANK_1;
    eraseInitStruct.Page = kEepromStartPage;
    eraseInitStruct.NbPages = kEepromPages;

    if (HAL_FLASHEx_Erase(&eraseInitStruct, &pageError) != HAL_OK)
    {
        success = false;
    }
    else
    {
        for (uint32_t offset = 0; offset < EEPROM_SIZE; offset += sizeof(uint64_t))
        {
            uint64_t dataWord = 0xFFFFFFFFFFFFFFFFULL;
            uint32_t copyLen = (EEPROM_SIZE - offset) >= sizeof(uint64_t)
                                   ? sizeof(uint64_t)
                                   : (EEPROM_SIZE - offset);
            std::memcpy(&dataWord, eepromBuffer + offset, copyLen);
            if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                                  kEepromBaseAddress + offset,
                                  dataWord) != HAL_OK)
            {
                success = false;
                break;
            }
        }
    }

    if (HAL_FLASH_Lock() != HAL_OK)
    {
        success = false;
    }

    if (success &&
        std::memcmp(eepromBuffer,
                    reinterpret_cast<const void*>(kEepromBaseAddress),
                    EEPROM_SIZE) != 0)
    {
        success = false;
    }

    return success;
}

uint8_t EEPROMReadBufferedByte(uint32_t _pos)
{
    if (_pos >= EEPROM_SIZE)
    {
        return 0xFFU;
    }
    return eepromBuffer[_pos];
}

void EEPROMWriteBufferedByte(uint32_t _pos, uint8_t _value)
{
    if (_pos >= EEPROM_SIZE)
    {
        return;
    }
    eepromBuffer[_pos] = _value;
}
