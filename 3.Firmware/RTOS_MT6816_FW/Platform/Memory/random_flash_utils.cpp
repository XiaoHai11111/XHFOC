#ifndef __STM32_EEPROM_HPP
#define __STM32_EEPROM_HPP

#include <cstdint>
#include <cstring>
#include "stm32g4xx_hal.h"  // STM32G4系列头文件
#include "random_flash_utils.h"

// ====================== STM32G431RBT6 专用配置 ======================
// 芯片规格：128KB Flash, 2KB/页，单Bank
#define FLASH_BASE_ADDR        0x08000000U
#define FLASH_SIZE_KB          128U                    // STM32G431RBT6 有128KB Flash
#define FLASH_SIZE_BYTES       (FLASH_SIZE_KB * 1024U)
#define FLASH_END_ADDR         (FLASH_BASE_ADDR + FLASH_SIZE_BYTES - 1)
#define FLASH_TOTAL_PAGES      (FLASH_SIZE_BYTES / FLASH_PAGE_SIZE)  // 共64页

// EEPROM模拟配置（使用最后2页，共4KB空间）
#define EEPROM_PAGES           2                      // 占用2页
#define EEPROM_START_PAGE      (FLASH_TOTAL_PAGES - EEPROM_PAGES)  // 从倒数第2页开始
#define EEPROM_BASE_ADDRESS    (FLASH_BASE_ADDR + (EEPROM_START_PAGE * FLASH_PAGE_SIZE))

// 缓冲区（STM32G431RBT6无CCMRAM，使用普通RAM）
static uint8_t eepromBuffer[EEPROM_SIZE] __attribute__((aligned(8))) = {0};
// ====================================================================

/**
 * @brief  从模拟EEPROM读取一个字节
 * @param  _pos 字节位置(0 ~ EEPROM_SIZE-1)
 * @return 读取的字节值
 */
uint8_t EEPROMReadByte(const uint32_t _pos)
{
    EEPROMFillBuffer();
    return EEPROMReadBufferedByte(_pos);
}

/**
 * @brief  写入一个字节到模拟EEPROM
 * @param  _pos 字节位置(0 ~ EEPROM_SIZE-1)
 * @param  _value 要写入的字节值
 * @note   每次写入都会擦除并重写整个EEPROM区域，频繁操作将影响Flash寿命
 */
void EEPROMWriteByte(uint32_t _pos, uint8_t _value)
{
    EEPROMWriteBufferedByte(_pos, _value);
    EEPROMBufferFlush();
}

/**
 * @brief  从缓冲区读取一个字节（不涉及Flash操作）
 * @param  _pos 字节位置
 * @return 字节值
 */
uint8_t EEPROMReadBufferedByte(const uint32_t _pos)
{
    if (_pos >= EEPROM_SIZE) return 0xFF;  // 边界检查
    return eepromBuffer[_pos];
}

/**
 * @brief  写入一个字节到缓冲区（不立即写入Flash）
 * @param  _pos 字节位置
 * @param  _value 字节值
 */
void EEPROMWriteBufferedByte(uint32_t _pos, uint8_t _value)
{
    if (_pos >= EEPROM_SIZE) return;  // 边界检查
    eepromBuffer[_pos] = _value;
}

/**
 * @brief  从Flash加载数据到RAM缓冲区
 */
void EEPROMFillBuffer(void)
{
    memcpy(eepromBuffer, (uint8_t*)(EEPROM_BASE_ADDRESS), EEPROM_SIZE);
}

/**
 * @brief  将缓冲区数据写入Flash（STM32G431RBT6专用实现）
 * @note   此操作会擦除最后2页Flash，请勿频繁调用
 */
void EEPROMBufferFlush(void)
{
    FLASH_EraseInitTypeDef eraseInitStruct = {0};
    uint32_t pageError = 0;
    uint32_t currentAddress = EEPROM_BASE_ADDRESS;

    // 1. 解锁Flash
    HAL_FLASH_Unlock();

    // 2. 配置擦除参数
    eraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
    eraseInitStruct.Banks = FLASH_BANK_1;              // G431为单Bank
    eraseInitStruct.Page = EEPROM_START_PAGE;          // 起始页号：62
    eraseInitStruct.NbPages = EEPROM_PAGES;            // 擦除页数：2

    // 3. 执行页擦除
    if (HAL_FLASHEx_Erase(&eraseInitStruct, &pageError) == HAL_OK)
    {
        // 4. 按字(32-bit)编程
        for (uint32_t offset = 0; offset < EEPROM_SIZE; offset += 4)
        {
            uint64_t dataWord = 0;

            // 从缓冲区提取4字节（可能包含1-4个有效字节）
            memcpy(&dataWord, eepromBuffer + offset, sizeof(uint32_t));

            // 编程到Flash
            if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                                  currentAddress + offset,
                                  dataWord) != HAL_OK)
            {
                // 编程失败处理
                break;
            }
        }
    }

    // 5. 锁定Flash
    HAL_FLASH_Lock();
}

/**
 * @brief  批量读取数据
 * @param  dest 目标缓冲区
 * @param  srcPos 源起始位置(0 ~ EEPROM_SIZE-1)
 * @param  len 读取长度
 */
void EEPROMReadBytes(uint8_t* dest, uint32_t srcPos, uint32_t len)
{
    if (!dest || srcPos + len > EEPROM_SIZE) return;

    EEPROMFillBuffer();
    memcpy(dest, eepromBuffer + srcPos, len);
}

/**
 * @brief  批量写入数据
 * @param  src 源数据缓冲区
 * @param  destPos 目标起始位置(0 ~ EEPROM_SIZE-1)
 * @param  len 写入长度
 * @note   此操作会擦除整个EEPROM区域，建议累积足够数据后一次性写入
 */
void EEPROMWriteBytes(const uint8_t* src, uint32_t destPos, uint32_t len)
{
    if (!src || destPos + len > EEPROM_SIZE) return;

    EEPROMFillBuffer();
    memcpy(eepromBuffer + destPos, src, len);
    EEPROMBufferFlush();
}

/**
 * @brief  擦除整个EEPROM区域（填充0xFF）
 */
void EEPROMEraseAll(void)
{
    memset(eepromBuffer, 0xFF, EEPROM_SIZE);
    EEPROMBufferFlush();
}

/**
 * @brief  获取EEPROM状态信息
 */
void EEPROMGetInfo(uint32_t* baseAddr, uint32_t* size, uint32_t* startPage)
{
    if (baseAddr) *baseAddr = EEPROM_BASE_ADDRESS;
    if (size) *size = EEPROM_SIZE;
    if (startPage) *startPage = EEPROM_START_PAGE;
}

#endif