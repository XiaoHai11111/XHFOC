#include "common_inc.h"
#include <string>


void OnUsbAsciiCmd(const char* _cmd, size_t _len, StreamSink &_responseChannel)
{
    /*---------------------------- ↓ Add Your CMDs Here ↓ -----------------------------*/
    if (_cmd[0] == '!' )
    {
        std::string s(_cmd);
        if (s.find("STOP") != std::string::npos)
        {
            Respond(_responseChannel, "Stopped ok");
        } else if (s.find("START") != std::string::npos)
        {
            Respond(_responseChannel, "Started ok");
        } else if (s.find("DISABLE") != std::string::npos)
        {
            Respond(_responseChannel, "Disabled ok");
        }
    }

/*---------------------------- ↑ Add Your CMDs Here ↑ -----------------------------*/
}


void OnUart3AsciiCmd(const char* _cmd, size_t _len, StreamSink &_responseChannel)
{
    /*---------------------------- ↓ Add Your CMDs Here ↓ -----------------------------*/
    if (_cmd[0] == '!')
    {
        std::string s(_cmd);
        if (s.find("STOP") != std::string::npos)
        {
            Respond(_responseChannel, "Stopped ok");
        } else if (s.find("START") != std::string::npos)
        {
            Respond(_responseChannel, "Started ok");
        } else if (s.find("DISABLE") != std::string::npos)
        {
            Respond(_responseChannel, "Disabled ok");
        }
    }
/*---------------------------- ↑ Add Your CMDs Here ↑ -----------------------------*/
}