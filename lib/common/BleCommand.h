#ifndef BLE_COMMAND_H
#define BLE_COMMAND_H

#include "Command.h"
#include <string>

class BLEController;

class BleCommand : public Command {
public:
    BleCommand(BLEController* bleController, const std::string& action);
    void press() override;
    void release() override;

private:
    BLEController* _bleController;
    std::string _action;
};

#endif // BLE_COMMAND_H
