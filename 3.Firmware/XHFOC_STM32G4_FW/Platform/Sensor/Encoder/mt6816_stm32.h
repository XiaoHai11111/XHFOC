#ifndef CTRL_STEP_FW_MT6816_STM32_H
#define CTRL_STEP_FW_MT6816_STM32_H

#include "mt6816_base.h"
#include "spi.h"

class MT6816 : public MT6816Base
{
public:
    /*
     * _quickCaliDataPtr is the start address where calibration data stored,
     * in STM32F103CBT6 flash size is 128K (0x08000000 ~ 0x08020000), we use
     * last 33K (32K clib + 1K user) for storage, and it starts from 0x08017C00.
     */
    explicit MT6816(SPI_HandleTypeDef* _spiHandle = &hspi1) :
        MT6816Base((uint16_t*) (0x08017C00)),
        spiHandle(_spiHandle)
    {}

    // Called from the SPI1 interrupt (HAL_SPI_TxRxCpltCallback) to advance the
    // non-blocking read state machine. Public so the ISR shim can reach it.
    void OnSpiTxRxComplete();

    // Enable the non-blocking (pipelined) read path. Must stay disabled during
    // init/sensor alignment (sparse reads need fresh, synchronous values);
    // enable it once the high-rate FOC loop starts.
    void EnableAsyncRead(bool _enable) { asyncEnabled_ = _enable; }


private:
    enum SpiState_t
    {
        MT_IDLE = 0,
        MT_FRAME0,
        MT_FRAME1
    };

    SPI_HandleTypeDef* spiHandle = nullptr;

    // Non-blocking (interrupt-driven) read pipeline state.
    volatile SpiState_t spiState_ = MT_IDLE;
    uint16_t txFrame_[2] = { (uint16_t) ((0x80 | 0x03) << 8),
                             (uint16_t) ((0x80 | 0x04) << 8) };
    uint16_t rxFrame_[2] = {0, 0};
    volatile uint16_t asyncRawData_ = 0;
    volatile bool sampleReady_ = false;
    bool asyncEnabled_ = false;

    void SpiInit() override;

    uint16_t SpiTransmitAndRead16Bits(uint16_t _data) override;

    // Runtime path: consume the last completed background sample and immediately
    // start the next transfer, so the FOC loop never busy-waits on SPI.
    float GetRawAngle() override;
    void StartRead();
};

#endif
