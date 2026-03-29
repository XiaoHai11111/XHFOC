#ifndef __ASCII_PROTOCOL_H
#define __ASCII_PROTOCOL_H


/* Includes ------------------------------------------------------------------*/
#include <protocol.hpp>

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos_inc.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported variables --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/
void ASCII_protocol_parse_stream(const uint8_t* buffer, size_t len, StreamSink& response_channel);
void OnUsbAsciiCmd(const char* _cmd, size_t _len, StreamSink& _responseChannel);
void OnUart3AsciiCmd(const char* _cmd, size_t _len, StreamSink& _responseChannel);

// Function to send messages back through specific channel (UART or USB-VCP).
// Use this function instead of printf because printf will send messages over ALL CHANNEL.
template<typename ... TArgs>
void Respond(StreamSink &output , const char *fmt, TArgs &&... args)
{
    if (log_mutex != nullptr)
    {
        (void)osMutexAcquire(log_mutex, osWaitForever);
    }

    char response[192];
    int n = snprintf(response, sizeof(response), fmt, std::forward<TArgs>(args)...);
    if (n < 0)
    {
        if (log_mutex != nullptr)
        {
            (void)osMutexRelease(log_mutex);
        }
        return;
    }
    size_t len = (n >= (int)sizeof(response)) ? (sizeof(response) - 1U) : (size_t)n;
    output.process_bytes((const uint8_t *)response, len, nullptr);
    output.process_bytes((const uint8_t *) "\r\n", 2, nullptr);

    if (log_mutex != nullptr)
    {
        (void)osMutexRelease(log_mutex);
    }
}


#endif /* __ASCII_PROTOCOL_H */
