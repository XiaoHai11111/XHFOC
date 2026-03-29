#include "mt6816_stm32.h"

void MT6816::SpiInit()
{
    MX_SPI1_Init();
}

uint16_t MT6816::SpiTransmitAndRead16Bits(uint16_t _dataTx)
{
    uint16_t dataRx = 0;
    if (spiHandle == nullptr)
        return dataRx;

    HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_RESET);
    (void)HAL_SPI_TransmitReceive(spiHandle, (uint8_t*) &_dataTx, (uint8_t*) &dataRx, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_SET);

    return dataRx;
}
