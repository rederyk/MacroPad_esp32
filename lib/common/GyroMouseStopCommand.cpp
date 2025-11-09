#include "GyroMouseStopCommand.h"
#include "GyroMouse.h"
#include "macroManager.h"

GyroMouseStopCommand::GyroMouseStopCommand(GyroMouse* gyroMouse, MacroManager* macroManager)
    : _gyroMouse(gyroMouse), _macroManager(macroManager) {}

void GyroMouseStopCommand::press() {
    if (!_gyroMouse || !_macroManager) return;

    if (_gyroMouse->isRunning()) {
        _gyroMouse->stop();
    }

    if (_macroManager->isGyroModeActive() && _macroManager->hasSavedGyroCombo()) {
        _macroManager->restoreSavedGyroCombo();
    }

    _macroManager->setGyroModeActive(false);
}

void GyroMouseStopCommand::release() {
    // No action on release
}
