#include "BleCommand.h"
#include "BLEController.h"

BleCommand::BleCommand(BLEController* bleController, const std::string& action)
    : _bleController(bleController), _action(action) {}

void BleCommand::press() {
    if (_bleController && _bleController->isBleEnabled()) {
        _bleController->BLExecutor(_action.c_str(), true);
    }
}

void BleCommand::release() {
    if (_bleController && _bleController->isBleEnabled()) {
        _bleController->BLExecutor(_action.c_str(), false);
    }
}
