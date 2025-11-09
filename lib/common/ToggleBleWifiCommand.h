#ifndef TOGGLE_BLE_WIFI_COMMAND_H
#define TOGGLE_BLE_WIFI_COMMAND_H

#include "Command.h"
#include "specialAction.h"

class ToggleBleWifiCommand : public Command {
public:
    ToggleBleWifiCommand(SpecialAction* specialAction);
    void press() override;
    void release() override;

private:
    SpecialAction* _specialAction;
};

#endif // TOGGLE_BLE_WIFI_COMMAND_H