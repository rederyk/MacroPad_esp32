#include "GyroMouseCycleSensitivityCommand.h"
#include "GyroMouse.h"
#include "Logger.h"

GyroMouseCycleSensitivityCommand::GyroMouseCycleSensitivityCommand(GyroMouse* gyroMouse)
    : _gyroMouse(gyroMouse) {}

void GyroMouseCycleSensitivityCommand::press() {
    if (!_gyroMouse) return;

    if (_gyroMouse->isRunning()) {
        _gyroMouse->cycleSensitivity();
        Logger::getInstance().log("GyroMouse: Sensitivity -> " + _gyroMouse->getSensitivityName());
    } else {
        Logger::getInstance().log("GyroMouse: Cycle request ignored (mode inactive)");
    }
}

void GyroMouseCycleSensitivityCommand::release() {
    // No action on release
}
