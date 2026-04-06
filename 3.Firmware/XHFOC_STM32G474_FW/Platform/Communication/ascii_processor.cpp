/*
* The ASCII protocol is a simpler, human readable alternative to the main native
* protocol.
* In the future this protocol might be extended to support selected GCode commands.
* For a list of supported commands see doc/ascii-protocol.md
*/

/* Includes ------------------------------------------------------------------*/

#include "common_inc.h"
#include "ascii_processor.hpp"

/* Private macros ------------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Global constant data ------------------------------------------------------*/
/* Global variables ----------------------------------------------------------*/
/* Private constant data -----------------------------------------------------*/
#define MAX_LINE_LENGTH 256
#define TO_STR_INNER(s) #s
#define TO_STR(s) TO_STR_INNER(s)
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Function implementations --------------------------------------------------*/


// @brief Executes an ASCII protocol command
// @param buffer buffer of ASCII encoded characters
// @param len size of the buffer
void ASCII_protocol_process_line(const uint8_t* buffer, size_t len, StreamSink &response_channel)
{
    static_assert(sizeof(char) == sizeof(uint8_t));

    // copy everything into a local buffer so we can insert null-termination
    char cmd[MAX_LINE_LENGTH + 1];
    if (len > MAX_LINE_LENGTH) len = MAX_LINE_LENGTH;
    memcpy(cmd, buffer, len);

    cmd[len] = 0; // null-terminate

    if (response_channel.channelType == StreamSink::CHANNEL_TYPE_USB)
        OnUsbAsciiCmd(cmd, len, response_channel);
    else if (response_channel.channelType == StreamSink::CHANNEL_TYPE_UART3)
        OnUart3AsciiCmd(cmd, len, response_channel);
}

void ASCII_protocol_parse_stream(const uint8_t* buffer, size_t len, StreamSink &response_channel)
{
    struct AsciiParseState {
        uint8_t parse_buffer[MAX_LINE_LENGTH];
        bool initialized;
        bool read_active;
        uint32_t parse_buffer_idx;
    };
    static AsciiParseState parse_states[StreamSink::CHANNEL_TYPE_UART5 + 1] = {};

    uint32_t channel_idx = static_cast<uint32_t>(response_channel.channelType);
    if (channel_idx > static_cast<uint32_t>(StreamSink::CHANNEL_TYPE_UART5)) {
        channel_idx = static_cast<uint32_t>(StreamSink::CHANNEL_TYPE_USB);
    }
    AsciiParseState &state = parse_states[channel_idx];
    if (!state.initialized) {
        state.initialized = true;
        state.read_active = true;
    }

    while (len--)
    {
        // if the line becomes too long, reset buffer and wait for the next line
        if (state.parse_buffer_idx >= MAX_LINE_LENGTH)
        {
            state.read_active = false;
            state.parse_buffer_idx = 0;
        }

        // Fetch the next char
        uint8_t c = *(buffer++);
        bool is_end_of_line = (c == '\r' || c == '\n');
        if (is_end_of_line)
        {
            if (state.read_active)
                ASCII_protocol_process_line(state.parse_buffer, state.parse_buffer_idx, response_channel);
            state.parse_buffer_idx = 0;
            state.read_active = true;
        } else
        {
            if (state.read_active)
            {
                state.parse_buffer[state.parse_buffer_idx++] = c;
            }
        }
    }
}
