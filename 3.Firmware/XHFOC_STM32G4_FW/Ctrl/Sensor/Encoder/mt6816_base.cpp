#include "mt6816_base.h"
#include "math_utils.h"

bool MT6816Base::Init()
{
    SpiInit();
    ReadRawData();

    // Check if the stored calibration data are valid
    angleData.rectifyValid = true;
    for (int32_t i = 0; i < RESOLUTION; i++)
    {
        if (quickCaliDataPtr[i] == 0xFFFF)
            angleData.rectifyValid = false;
    }

    if (angleData.rectifyValid)
    {
        angleData.rectifiedAngle = quickCaliDataPtr[angleData.rawAngle];
    }
    else
    {
        angleData.rectifiedAngle = angleData.rawAngle;
    }

    VarInit();

    return angleData.rectifyValid;
}


uint16_t MT6816Base::UpdateAngle()
{
    ReadRawData();
    return angleData.rectifiedAngle;
}

bool MT6816Base::ApplyRawData(uint16_t rawData16)
{
    spiRawData.rawData = rawData16;

    //奇偶校验
    hCount = 0;
    for (uint8_t j = 0; j < 16; j++)
    {
        if (spiRawData.rawData & (0x0001 << j))
            hCount++;
    }
    if (hCount & 0x01)
    {
        // Parity failed: keep the last valid angle, report invalid checksum.
        spiRawData.checksumFlag = false;
        return false;
    }

    spiRawData.checksumFlag = true;
    spiRawData.rawAngle = spiRawData.rawData >> 2;
    spiRawData.noMagFlag = (bool) (spiRawData.rawData & (0x0001 << 1));
    angleData.rawAngle = spiRawData.rawAngle;
    angleData.rectifiedAngle = angleData.rectifyValid
                               ? quickCaliDataPtr[angleData.rawAngle]
                               : angleData.rawAngle;
    return true;
}

void MT6816Base::ReadRawData()
{
    dataTx[0] = (0x80 | 0x03) << 8;
    dataTx[1] = (0x80 | 0x04) << 8;

    for (uint8_t i = 0; i < 3; i++)
    {
        dataRx[0] = SpiTransmitAndRead16Bits(dataTx[0]);
        dataRx[1] = SpiTransmitAndRead16Bits(dataTx[1]);

        if (ApplyRawData(((dataRx[0] & 0x00FF) << 8) | (dataRx[1] & 0x00FF)))
            break;
    }
}


bool MT6816Base::IsCalibrated()
{
    return angleData.rectifyValid;
}

float MT6816Base::GetRawAngle()
{
    const uint16_t angle = UpdateAngle();
    return (static_cast<float>(angle) / static_cast<float>(RESOLUTION)) * _2PI;
}
