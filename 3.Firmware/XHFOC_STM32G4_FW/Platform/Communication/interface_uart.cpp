#include "common_inc.h"
#include "interface_uart.hpp"
#include "ascii_processor.hpp"
#include "protocol.hpp"
#include "usart.h"

#define UART_TX_BUFFER_SIZE 64
#define UART_RX_BUFFER_SIZE 128

// DMA open-loop continuous circular buffer. IDLE, half-transfer and
// transfer-complete events wake the parser task.
static uint8_t dma_rx_buffer[UART_RX_BUFFER_SIZE];
static uint32_t dma_last_rcv_idx;
static volatile uint32_t dma_rx_event_idx;

static constexpr uint32_t UART_RX_DATA_EVENT = 1U << 0;
static constexpr uint32_t UART_RX_ERROR_EVENT = 1U << 1;
static constexpr uint32_t UART_RX_EVENT_MASK = UART_RX_DATA_EVENT | UART_RX_ERROR_EVENT;

// FIXME: the stdlib doesn't know about CMSIS threads, so this is just a global variable
// static thread_local uint32_t deadline_ms = 0;

osThreadId_t uartServerTaskHandle;


class UART3Sender : public StreamSink
{
public:
    UART3Sender()
    {
        channelType = CHANNEL_TYPE_UART3;
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
} uart3_stream_output;

StreamSink* uart3StreamOutputPtr = &uart3_stream_output;
StreamBasedPacketSink uart3_packet_output(uart3_stream_output);
BidirectionalPacketBasedChannel uart3_channel(uart3_packet_output);
StreamToPacketSegmenter uart3_stream_input(uart3_channel);

static void ProcessUartRxTo(uint32_t new_rcv_idx)
{
    // Process bytes in one or two chunks (two in case there was a wrap).
    if (new_rcv_idx < dma_last_rcv_idx)
    {
        uart3_stream_input.process_bytes(dma_rx_buffer + dma_last_rcv_idx,
                                         UART_RX_BUFFER_SIZE - dma_last_rcv_idx,
                                         nullptr); // TODO: use process_all
        ASCII_protocol_parse_stream(dma_rx_buffer + dma_last_rcv_idx,
                                    UART_RX_BUFFER_SIZE - dma_last_rcv_idx, uart3_stream_output);
        dma_last_rcv_idx = 0;
    }
    if (new_rcv_idx > dma_last_rcv_idx)
    {
        uart3_stream_input.process_bytes(dma_rx_buffer + dma_last_rcv_idx,
                                         new_rcv_idx - dma_last_rcv_idx,
                                         nullptr); // TODO: use process_all
        ASCII_protocol_parse_stream(dma_rx_buffer + dma_last_rcv_idx,
                                    new_rcv_idx - dma_last_rcv_idx, uart3_stream_output);
        dma_last_rcv_idx = new_rcv_idx;
    }
}

static bool StartUartRxDma()
{
    dma_last_rcv_idx = 0;
    dma_rx_event_idx = 0;
    return HAL_UARTEx_ReceiveToIdle_DMA(&huart3, dma_rx_buffer, sizeof(dma_rx_buffer)) == HAL_OK;
}

static void UartServerTask(void* ctx)
{
    (void) ctx;

    // The task can preempt its creator before osThreadNew() returns, so publish
    // the current handle before enabling callbacks from the RX DMA.
    uartServerTaskHandle = osThreadGetId();

    while (!StartUartRxDma())
    {
        osDelay(10);
    }

    for (;;)
    {
        const uint32_t events =
                osThreadFlagsWait(UART_RX_EVENT_MASK, osFlagsWaitAny, osWaitForever);
        if ((events & osFlagsError) != 0U)
        {
            continue;
        }

        if ((events & UART_RX_ERROR_EVENT) != 0U)
        {
            (void)HAL_UART_AbortReceive(&huart3);
            (void)osThreadFlagsClear(UART_RX_EVENT_MASK);
            while (!StartUartRxDma())
            {
                osDelay(10);
            }
            continue;
        }

        if ((events & UART_RX_DATA_EVENT) != 0U)
        {
            ProcessUartRxTo(dma_rx_event_idx);
        }
    }
}

const osThreadAttr_t uartServerTask_attributes = {
    .name = "UartServerTask",
    .stack_size = 1280,
    .priority = (osPriority_t) osPriorityAboveNormal,
};

void StartUartServer()
{
    // The server task starts circular Receive-to-IDLE DMA and blocks on RX events.
    uartServerTaskHandle = osThreadNew(UartServerTask, nullptr, &uartServerTask_attributes);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart)
{
    if (huart->Instance == USART3)
        osSemaphoreRelease(sem_uart3_dma);
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t size)
{
    if (huart->Instance != USART3)
        return;

    dma_rx_event_idx = (size >= UART_RX_BUFFER_SIZE) ? 0U : size;
    if (uartServerTaskHandle != nullptr)
        (void)osThreadFlagsSet(uartServerTaskHandle, UART_RX_DATA_EVENT);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart)
{
    if ((huart->Instance == USART3) && (uartServerTaskHandle != nullptr))
        (void)osThreadFlagsSet(uartServerTaskHandle, UART_RX_ERROR_EVENT);
}
