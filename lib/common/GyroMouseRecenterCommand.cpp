#include "GyroMouseRecenterCommand.h"
#include "GyroMouse.h"
#include "Logger.h"

GyroMouseRecenterCommand::GyroMouseRecenterCommand(GyroMouse* gyroMouse)
    : _gyroMouse(gyroMouse) {}

void GyroMouseRecenterCommand::press() {
    if (!_gyroMouse) return;

    if (_gyroMouse->isRunning()) {
        _gyroMouse->recenterNeutral();
    } else {
        Logger::getInstance().log("GyroMouse: Recenter request ignored (mode inactive)");
    }
}

void GyroMouseRecenterCommand::release() {
    // No action on release
}
