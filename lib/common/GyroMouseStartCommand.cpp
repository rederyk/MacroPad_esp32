#include "GyroMouseStartCommand.h"
#include "GyroMouse.h"
#include "macroManager.h"
#include "Logger.h"

GyroMouseStartCommand::GyroMouseStartCommand(GyroMouse* gyroMouse, MacroManager* macroManager)
    : _gyroMouse(gyroMouse), _macroManager(macroManager) {}

void GyroMouseStartCommand::press() {
    if (!_gyroMouse || !_macroManager) return;

    if (!_gyroMouse->isRunning()) {
        if (!_macroManager->isGyroModeActive()) {
            _macroManager->saveCurrentComboForGyro();
        }

        _gyroMouse->start();
        bool isRunning = _gyroMouse->isRunning();
        _macroManager->setGyroModeActive(isRunning);

        if (isRunning) {
            _macroManager->setPendingComboSwitch("combo_gyromouse", 0);
        } else {
            Logger::getInstance().log("GyroMouse: failed to start (check configuration)");
        }
    }
}

void GyroMouseStartCommand::release() {
    // No action on release
}
