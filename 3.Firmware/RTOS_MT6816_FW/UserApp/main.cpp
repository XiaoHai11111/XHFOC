#include "common_inc.h"
#include "cmd_ctrl_motor.h"
/* Default Entry -------------------------------------------------------*/
CmdCtrlMotor* motor = new CmdCtrlMotor( 3, true, 30, 35, 180);

void Main(void)
{
    // Init all communication staff, including USB-CDC/VCP/UART/CAN etc.
    InitCommunication();
    Respond(*uart3StreamOutputPtr, "[sys] Heap remain: %d Bytes\n", xPortGetMinimumEverFreeHeapSize());
}
