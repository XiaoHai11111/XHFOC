#ifndef XHFOC_ENCODER_CALIBRATION_STORAGE_H
#define XHFOC_ENCODER_CALIBRATION_STORAGE_H

#include <cstdint>

class EncoderCalibrationStorage
{
public:
    struct Data
    {
        float zeroElectricAngleOffset = 0.0f;
        int32_t sensorDirection = 0;
    };

    enum class LoadResult
    {
        VALID,
        EMPTY,
        INVALID
    };

    LoadResult Load(uint32_t expectedPolePairs, Data& data);
    bool Save(uint32_t polePairs, const Data& data);
    bool Clear();
};

#endif
