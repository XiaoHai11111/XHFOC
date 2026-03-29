#ifndef __STM32_EEPROM_H
#define __STM32_EEPROM_H

#ifdef __cplusplus
extern "C" {
#include <stm32g4xx.h>
#include <stm32g4xx_hal_flash_ex.h>
#endif

#define EEPROM_SIZE  ((uint32_t)(1*1024))


uint8_t EEPROMReadByte(uint32_t _pos);
void EEPROMWriteByte(uint32_t _pos, uint8_t _value);

void EEPROMFillBuffer();
void EEPROMBufferFlush();
uint8_t EEPROMReadBufferedByte(uint32_t _pos);
void EEPROMWriteBufferedByte(uint32_t _pos, uint8_t _value);


#ifdef __cplusplus
}
#endif
#endif
