#include "encoder_calibration_storage.h"

#include "random_flash_interface.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {
constexpr uint32_t kRecordOffset = 0U;
constexpr uint32_t kMagic = 0x58484341U;  // "XHCA"
constexpr uint16_t kSchemaVersion = 1U;
constexpr float kTwoPi = 6.28318530717958647692f;

struct PersistentRecord
{
    uint32_t magic;
    uint16_t schemaVersion;
    uint16_t recordSize;
    uint32_t polePairs;
    float zeroElectricAngleOffset;
    int32_t sensorDirection;
    uint32_t crc32;
};

static_assert(sizeof(PersistentRecord) == 24U, "Persistent calibration layout changed");
static_assert(offsetof(PersistentRecord, crc32) == 20U, "CRC must be the final record field");

uint32_t CalculateCrc32(const void* data, size_t length)
{
    const auto* bytes = static_cast<const uint8_t*>(data);
    uint32_t crc = 0xFFFFFFFFU;

    for (size_t i = 0; i < length; ++i)
    {
        crc ^= bytes[i];
        for (uint8_t bit = 0; bit < 8U; ++bit)
        {
            const uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }

    return ~crc;
}

bool IsErased(const PersistentRecord& record)
{
    const auto* bytes = reinterpret_cast<const uint8_t*>(&record);
    for (size_t i = 0; i < sizeof(record); ++i)
    {
        if (bytes[i] != 0xFFU)
        {
            return false;
        }
    }
    return true;
}

bool IsDataValid(uint32_t polePairs, float zeroElectricAngleOffset, int32_t sensorDirection)
{
    return polePairs > 0U &&
           std::isfinite(zeroElectricAngleOffset) &&
           zeroElectricAngleOffset >= 0.0f &&
           zeroElectricAngleOffset <= kTwoPi &&
           (sensorDirection == 1 || sensorDirection == -1);
}
}

EncoderCalibrationStorage::LoadResult EncoderCalibrationStorage::Load(uint32_t expectedPolePairs,
                                                                      Data& data)
{
    EEPROM eeprom;
    PersistentRecord record{};
    eeprom.Pull(static_cast<int>(kRecordOffset), record);

    if (!eeprom.isValid)
    {
        return LoadResult::INVALID;
    }
    if (IsErased(record))
    {
        return LoadResult::EMPTY;
    }

    const uint32_t expectedCrc = CalculateCrc32(&record, offsetof(PersistentRecord, crc32));
    if (record.magic != kMagic ||
        record.schemaVersion != kSchemaVersion ||
        record.recordSize != sizeof(PersistentRecord) ||
        record.polePairs != expectedPolePairs ||
        record.crc32 != expectedCrc ||
        !IsDataValid(record.polePairs,
                     record.zeroElectricAngleOffset,
                     record.sensorDirection))
    {
        return LoadResult::INVALID;
    }

    data.zeroElectricAngleOffset = record.zeroElectricAngleOffset;
    data.sensorDirection = record.sensorDirection;
    return LoadResult::VALID;
}

bool EncoderCalibrationStorage::Save(uint32_t polePairs, const Data& data)
{
    if (!IsDataValid(polePairs, data.zeroElectricAngleOffset, data.sensorDirection))
    {
        return false;
    }

    PersistentRecord record{};
    record.magic = kMagic;
    record.schemaVersion = kSchemaVersion;
    record.recordSize = sizeof(PersistentRecord);
    record.polePairs = polePairs;
    record.zeroElectricAngleOffset = data.zeroElectricAngleOffset;
    record.sensorDirection = data.sensorDirection;
    record.crc32 = CalculateCrc32(&record, offsetof(PersistentRecord, crc32));

    EEPROM eeprom;
    eeprom.SetCommitASAP(false);
    eeprom.Push(static_cast<int>(kRecordOffset), record);
    return eeprom.isValid && eeprom.Commit();
}

bool EncoderCalibrationStorage::Clear()
{
    PersistentRecord erasedRecord;
    std::memset(&erasedRecord, 0xFF, sizeof(erasedRecord));

    EEPROM eeprom;
    eeprom.SetCommitASAP(false);
    eeprom.Push(static_cast<int>(kRecordOffset), erasedRecord);
    return eeprom.isValid && eeprom.Commit();
}
