#include "vofa_debug.h"

#include <cstring>

bool VofaDebug::SendJustFloat(const float* values, size_t count)
{
    if ((output_ == nullptr) || (values == nullptr) || (count == 0U) || (count > kMaxChannels))
    {
        return false;
    }

    constexpr uint8_t kFrameTail[4] = {0x00U, 0x00U, 0x80U, 0x7FU};
    uint8_t frame[kMaxChannels * sizeof(float) + sizeof(kFrameTail)] = {0};
    const size_t payloadSize = count * sizeof(float);
    const size_t frameSize = payloadSize + sizeof(kFrameTail);

    std::memcpy(frame, values, payloadSize);
    std::memcpy(frame + payloadSize, kFrameTail, sizeof(kFrameTail));

    size_t processed = 0;
    const int status = output_->process_bytes(frame, frameSize, &processed);
    return (status == 0) && (processed == frameSize);
}
