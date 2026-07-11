#ifndef XHFOC_VOFA_DEBUG_H
#define XHFOC_VOFA_DEBUG_H

#include <cstddef>
#include <cstdint>
#include "protocol.hpp"

class VofaDebug
{
public:
    explicit VofaDebug(StreamSink* output = nullptr)
        : output_(output)
    {}

    void SetOutput(StreamSink* output)
    {
        output_ = output;
    }

    bool SendJustFloat(const float* values, size_t count);

private:
    static constexpr size_t kMaxChannels = 32U;
    StreamSink* output_ = nullptr;
};

#endif
