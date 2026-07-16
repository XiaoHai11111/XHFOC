#include "common_inc.h"
#include "cmd_ctrl_motor.h"
#include <cstring>

extern CmdCtrlMotor* motor;

static bool CommandEquals(const char* cmd, size_t len, const char* expected)
{
    const size_t expectedLen = std::strlen(expected);
    return len == expectedLen && std::memcmp(cmd, expected, expectedLen) == 0;
}

void OnUsbAsciiCmd(const char* _cmd, size_t _len, StreamSink &_responseChannel)
{
    /*---------------------------- ↓ Add Your CMDs Here ↓ -----------------------------*/
    if (_len == 0U || _cmd[0] != '!')
    {
        return;
    }

    if (CommandEquals(_cmd, _len, "!STOP"))
    {
        (void)motor->Stop();
        Respond(_responseChannel, "Stopped ok");
    }
    else if (CommandEquals(_cmd, _len, "!START"))
    {
        if (motor->Start())
        {
            Respond(_responseChannel, "Started ok");
        }
        else
        {
            Respond(_responseChannel, "Start rejected: motor not ready");
        }
    }
    else if (CommandEquals(_cmd, _len, "!DISABLE"))
    {
        // Reserved for a future DISARM implementation. It intentionally does
        // not change the motor state in this revision.
        Respond(_responseChannel, "DISABLE ignored");
    }

/*---------------------------- ↑ Add Your CMDs Here ↑ -----------------------------*/
}


void OnUart3AsciiCmd(const char* _cmd, size_t _len, StreamSink &_responseChannel)
{
    /*---------------------------- ↓ Add Your CMDs Here ↓ -----------------------------*/
    if (_len == 0U || _cmd[0] != '!')
    {
        return;
    }

    if (CommandEquals(_cmd, _len, "!STOP"))
    {
        (void)motor->Stop();
        Respond(_responseChannel, "Stopped ok");
    }
    else if (CommandEquals(_cmd, _len, "!START"))
    {
        if (motor->Start())
        {
            Respond(_responseChannel, "Started ok");
        }
        else
        {
            Respond(_responseChannel, "Start rejected: motor not ready");
        }
    }
/*---------------------------- ↑ Add Your CMDs Here ↑ -----------------------------*/
}
