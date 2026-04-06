
/* Includes ------------------------------------------------------------------*/

#include "communication.hpp"
#include "common_inc.h"

/* Private defines -----------------------------------------------------------*/
/* Private macros ------------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Global constant data ------------------------------------------------------*/
/* Global variables ----------------------------------------------------------*/
/* Private constant data -----------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
volatile bool endpointListValid = false;
/* Private function prototypes -----------------------------------------------*/
/* Function implementations --------------------------------------------------*/
// @brief Sends a line on the specified output.

osThreadId_t commTaskHandle;
const osThreadAttr_t commTask_attributes = {
    .name = "commTask",
    .stack_size = 1024,
    .priority = (osPriority_t) osPriorityNormal,
};

void InitCommunication(void)
{
    // Start command handling thread
    commTaskHandle = osThreadNew(CommunicationTask, nullptr, &commTask_attributes);

    while (!endpointListValid)
        osDelay(1);
}

extern PCD_HandleTypeDef hpcd_USB_FS;
osThreadId_t usbIrqTaskHandle;

void UsbDeferredInterruptTask(void* ctx)
{
    (void) ctx; // unused parameter

    for (;;)
    {
        // Wait for signalling from USB interrupt (OTG_FS_IRQHandler)
        osStatus semaphore_status = osSemaphoreAcquire(sem_usb_irq, osWaitForever);
        if (semaphore_status == osOK)
        {
            // We have a new incoming USB transmission: handle it
            HAL_PCD_IRQHandler(&hpcd_USB_FS);
            // Let the irq (OTG_FS_IRQHandler) fire again.
            HAL_NVIC_EnableIRQ(USB_LP_IRQn);
        }
    }
}

// Thread to handle deffered processing of USB interrupt, and
// read commands out of the UART DMA circular buffer
void CommunicationTask(void* ctx)
{
    (void) ctx; // unused parameter

    CommitProtocol();

    // Allow main init to continue
    endpointListValid = true;

    StartUartServer();
    StartUsbServer();

    for (;;)
    {
        osDelay(1000); // nothing to do
    }
}

extern "C" {
int _write(int file, const char* data, int len);
}

// @brief This is what printf calls internally
int _write(int file, const char* data, int len)
{
    if (log_mutex != nullptr)
    {
        (void)osMutexAcquire(log_mutex, osWaitForever);
    }

    usbStreamOutputPtr->process_bytes((const uint8_t*) data, len, nullptr);
    uart3StreamOutputPtr->process_bytes((const uint8_t*) data, len, nullptr);

    if (log_mutex != nullptr)
    {
        (void)osMutexRelease(log_mutex);
    }

    return len;
}
