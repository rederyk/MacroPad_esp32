#ifndef HOP_BLE_DEVICE_COMMAND_H
#define HOP_BLE_DEVICE_COMMAND_H

#include "Command.h"
#include "specialAction.h"

class HopBleDeviceCommand : public Command {
private:
    SpecialAction* _specialAction;

public:
    HopBleDeviceCommand(SpecialAction* specialAction) : _specialAction(specialAction) {}

    void press() override {
        if (_specialAction) {
            _specialAction->hopBleDevice();
        }
    }

    void release() override {
        // No action on release
    }
};

#endif // HOP_BLE_DEVICE_COMMAND_H
