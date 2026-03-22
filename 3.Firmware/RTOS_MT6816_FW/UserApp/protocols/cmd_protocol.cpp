
#include <string>
#include "cmd_ctrl_motor.h"
#include "common_inc.h"

/*----------------- 1.Add Your Extern Variables Here (Optional) ------------------*/
extern CmdCtrlMotor* motor;
class HelperFunctions
{
public:
    /*--------------- 2.Add Your Helper Functions Helper Here (optional) ----------------*/
    float getDeviceNum()
    { return 2.0; }

} staticFunctions;


// Define options that intractable with "reftool".
static inline auto MakeObjTree()
{
    /*--------------- 3.Add Your Protocol Variables & Functions Here ----------------*/
    return make_protocol_member_list(
        // Add Read-Only Variables
        make_protocol_ro_property("serial_number", &serialNumber),
        make_protocol_function("get_devicenum", staticFunctions, &HelperFunctions::getDeviceNum),
        make_protocol_object("motor", motor->MakeProtocolDefinitions())
    );
}


COMMIT_PROTOCOL
