#ifndef XHFOC_ENCODER_BASE_H
#define XHFOC_ENCODER_BASE_H

#include <cstdint>

class EncoderBase
{
public:
    typedef struct
    {
        uint16_t rawAngle;          // raw data
        uint16_t rectifiedAngle;    // calibrated rawAngle data
        bool rectifyValid;
    } AngleData_t;
    AngleData_t angleData{0};

    enum Direction
    {
        CW = 1,
        CCW = -1,
        UNKNOWN = 0
    };

    /*
     * Resolution is (2^14 = 16384), each state will use an uint16 data
     * as map, thus total need 32K-flash for calibration.
    */
    const int32_t RESOLUTION = ((int32_t) ((0x00000001U) << 14));

    virtual bool Init() = 0;
    virtual uint16_t UpdateAngle() = 0;
    virtual bool IsCalibrated() = 0;

    virtual void Update();
    virtual float GetLapAngle();
    virtual float GetFullAngle();
    virtual float GetVelocity();
    virtual int32_t GetRotationCount();

    Direction countDirection = Direction::UNKNOWN;

protected:
    virtual float GetRawAngle() = 0;
    virtual void VarInit();

    float angleLast = 0.0f;
    uint64_t angleTimestamp = 0;
    float velocityLast = 0.0f;
    uint64_t velocityTimestamp = 0;
    int32_t rotationCount = 0;
    int32_t rotationCountLast = 0;
};

#endif
