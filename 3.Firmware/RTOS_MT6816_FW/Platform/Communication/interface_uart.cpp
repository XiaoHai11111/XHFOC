#include "common_inc.h"
#include "interface_uart.hpp"
#include "ascii_processor.hpp"
#include "protocol.hpp"
#include "usart.h"

#define UART_TX_BUFFER_SIZE 64
#define UART_RX_BUFFER_SIZE 64

// DMA open loop continous circular buffer
// 1ms delay periodic, chase DMA ptr around
static uint8_t dma_rx_buffer[2][UART_RX_BUFFER_SIZE];
static uint32_t dma_last_rcv_idx[2];

// FIXME: the stdlib doesn't know about CMSIS threads, so this is just a global variable
// static thread_local uint32_t deadline_ms = 0;

osThreadId_t uartServerTaskHandle;


class UART3Sender : public StreamSink
{
public:
    UART3Sender()
    {
        channelType = CHANNEL_TYPE_UART1;
    }

    int process_bytes(const uint8_t* buffer, size_t length, size_t* processed_bytes) override
    {
        // Loop to ensure all bytes get sent
        while (length)
        {
            size_t chunk = length < UART_TX_BUFFER_SIZE ? length : UART_TX_BUFFER_SIZE;
            // wait for USB interface to become ready
            // TODO: implement ring buffer to get a more continuous stream of data
            // if (osSemaphoreWait(sem_uart_dma, deadline_to_timeout(deadline_ms)) != osOK)
            if (osSemaphoreAcquire(sem_uart3_dma, PROTOCOL_SERVER_TIMEOUT_MS) != osOK)
                return -1;
            // transmit chunk
            memcpy(tx_buf_, buffer, chunk);
            if (HAL_UART_Transmit_DMA(&huart3, tx_buf_, chunk) != HAL_OK)
                return -1;
            buffer += chunk;
            length -= chunk;
            if (processed_bytes)
                *processed_bytes += chunk;
        }
        return 0;
    }

    size_t get_free_space() override
    { return SIZE_MAX; }

private:
    uint8_t tx_buf_[UART_TX_BUFFER_SIZE];
} uart1_stream_output;

StreamSink* uart1StreamOutputPtr = &uart1_stream_output;
StreamBasedPacketSink uart1_packet_output(uart1_stream_output);
BidirectionalPacketBasedChannel uart1_channel(uart1_packet_output);
StreamToPacketSegmenter uart1_stream_input(uart1_channel);

static void UartServerTask(void* ctx)
{
    (void) ctx;

    for (;;)
    {
        // Check for UART errors and restart recieve DMA transfer if required
        if (huart3.ErrorCode != HAL_UART_ERROR_NONE)
        {
            HAL_UART_AbortReceive(&huart3);
            HAL_UART_Receive_DMA(&huart3, dma_rx_buffer[0], sizeof(dma_rx_buffer[0]));
        }
        // Fetch the circular buffer "write pointer", where it would write next
        uint32_t new_rcv_idx = UART_RX_BUFFER_SIZE - huart3.hdmarx->Instance->CNDTR;

        // deadline_ms = timeout_to_deadline(PROTOCOL_SERVER_TIMEOUT_MS);
        // Process bytes in one or two chunks (two in case there was a wrap)
        if (new_rcv_idx < dma_last_rcv_idx[0])
        {
            uart1_stream_input.process_bytes(dma_rx_buffer[0] + dma_last_rcv_idx[0],
                                             UART_RX_BUFFER_SIZE - dma_last_rcv_idx[0],
                                             nullptr); // TODO: use process_all
            ASCII_protocol_parse_stream(dma_rx_buffer[0] + dma_last_rcv_idx[0],
                                        UART_RX_BUFFER_SIZE - dma_last_rcv_idx[0], uart1_stream_output);
            dma_last_rcv_idx[0] = 0;
        }
        if (new_rcv_idx > dma_last_rcv_idx[0])
        {
            uart1_stream_input.process_bytes(dma_rx_buffer[0] + dma_last_rcv_idx[0],
                                             new_rcv_idx - dma_last_rcv_idx[0],
                                             nullptr); // TODO: use process_all
            ASCII_protocol_parse_stream(dma_rx_buffer[0] + dma_last_rcv_idx[0],
                                        new_rcv_idx - dma_last_rcv_idx[0], uart1_stream_output);
            dma_last_rcv_idx[0] = new_rcv_idx;
        }

        osDelay(1);
    };
}

const osThreadAttr_t uartServerTask_attributes = {
    .name = "UartServerTask",
    .stack_size = 2000,
    .priority = (osPriority_t) osPriorityNormal,
};

void StartUartServer()
{
    // DMA is set up to receive in a circular buffer forever.
    // We don't use interrupts to fetch the data, instead we periodically read
    // data out of the circular buffer into a parse buffer, controlled by a state machine
    HAL_UART_Receive_DMA(&huart3, dma_rx_buffer[0], sizeof(dma_rx_buffer[0]));
    dma_last_rcv_idx[0] = UART_RX_BUFFER_SIZE - huart3.hdmarx->Instance->CNDTR;

    // Start UART communication thread
    uartServerTaskHandle = osThreadNew(UartServerTask, nullptr, &uartServerTask_attributes);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart)
{
    if (huart->Instance == USART3)
        osSemaphoreRelease(sem_uart3_dma);
}
