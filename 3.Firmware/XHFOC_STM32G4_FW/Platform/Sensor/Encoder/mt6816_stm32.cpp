#include "mt6816_stm32.h"
#include "math_utils.h"

// Single MT6816 instance served by the SPI1 interrupt shim below.
static MT6816* g_mt6816Instance = nullptr;

void MT6816::SpiInit()
{
    MX_SPI1_Init();
    g_mt6816Instance = this;
    // SPI1 global interrupt is enabled in HAL_SPI_MspInit() (spi.c, USER CODE),
    // and routed by SPI1_IRQHandler() (stm32g4xx_it.c) -> HAL_SPI_IRQHandler().
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

void MT6816::StartRead()
{
    if (spiHandle == nullptr)
        return;
    // Skip if a transfer is still in flight (e.g. previous cycle not finished).
    if (spiState_ != MT_IDLE)
        return;
    if (HAL_SPI_GetState(spiHandle) != HAL_SPI_STATE_READY)
        return;

    spiState_ = MT_FRAME0;
    HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_RESET);
    if (HAL_SPI_TransmitReceive_IT(spiHandle, (uint8_t*) &txFrame_[0], (uint8_t*) &rxFrame_[0], 1) != HAL_OK)
    {
        HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_SET);
        spiState_ = MT_IDLE;
    }
}

void MT6816::OnSpiTxRxComplete()
{
    switch (spiState_)
    {
        case MT_FRAME0:
            // First register (0x03) done; pulse CS and read the second (0x04).
            HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_SET);
            spiState_ = MT_FRAME1;
            HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_RESET);
            if (HAL_SPI_TransmitReceive_IT(spiHandle, (uint8_t*) &txFrame_[1], (uint8_t*) &rxFrame_[1], 1) != HAL_OK)
            {
                HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_SET);
                spiState_ = MT_IDLE;
            }
            break;

        case MT_FRAME1:
            HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_SET);
            asyncRawData_ = (uint16_t) (((rxFrame_[0] & 0x00FF) << 8) | (rxFrame_[1] & 0x00FF));
            sampleReady_ = true;
            spiState_ = MT_IDLE;
            break;

        default:
            spiState_ = MT_IDLE;
            break;
    }
}

float MT6816::GetRawAngle()
{
    if (!asyncEnabled_)
    {
        // Synchronous blocking read. Used during init/sensor alignment, where
        // reads are separated by long HAL_Delay()s: the pipelined path would
        // return the position captured at the *previous* call (stale by ~1 s),
        // corrupting zeroElectricAngleOffset so the motor never commutates.
        for (uint8_t i = 0; i < 3; i++)
        {
            const uint16_t rx0 = SpiTransmitAndRead16Bits(txFrame_[0]);
            const uint16_t rx1 = SpiTransmitAndRead16Bits(txFrame_[1]);
            if (ApplyRawData((uint16_t) (((rx0 & 0x00FF) << 8) | (rx1 & 0x00FF))))
                break;
        }
    }
    else
    {
        // Non-blocking pipeline for the high-rate FOC loop: consume the last
        // completed background sample and immediately start the next transfer,
        // so the loop never busy-waits on SPI.
        if (sampleReady_)
        {
            sampleReady_ = false;
            ApplyRawData(asyncRawData_);
        }
        StartRead();
    }

    return (static_cast<float>(angleData.rectifiedAngle) / static_cast<float>(RESOLUTION)) * _2PI;
}

extern "C" void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef* hspi)
{
    if ((g_mt6816Instance != nullptr) && (hspi->Instance == SPI1))
    {
        g_mt6816Instance->OnSpiTxRxComplete();
    }
}
