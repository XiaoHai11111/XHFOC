#ifndef __STM32_EEPROM_H
#define __STM32_EEPROM_H

#ifdef __cplusplus
extern "C" {
#include <stm32g4xx.h>
#include <stm32g4xx_hal_flash_ex.h>
#endif

// Logical persistent-data capacity kept in RAM while a Flash page is updated.
// The physical erase unit is 2 KiB, but 256 B is enough for board settings and
// avoids permanently consuming 1 KiB of the STM32G431's 32 KiB SRAM.
#define EEPROM_SIZE  ((uint32_t)256U)


uint8_t EEPROMReadByte(uint32_t _pos);
bool EEPROMWriteByte(uint32_t _pos, uint8_t _value);

void EEPROMFillBuffer();
bool EEPROMBufferFlush();
uint8_t EEPROMReadBufferedByte(uint32_t _pos);
void EEPROMWriteBufferedByte(uint32_t _pos, uint8_t _value);


#ifdef __cplusplus
}
#endif
#endif
