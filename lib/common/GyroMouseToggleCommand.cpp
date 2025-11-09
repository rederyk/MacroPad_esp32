#include "GyroMouseToggleCommand.h"
#include "GyroMouse.h"
#include "macroManager.h"
#include "Logger.h"

GyroMouseToggleCommand::GyroMouseToggleCommand(GyroMouse* gyroMouse, MacroManager* macroManager)
    : _gyroMouse(gyroMouse), _macroManager(macroManager) {}

void GyroMouseToggleCommand::press() {
    if (!_gyroMouse || !_macroManager) return;

    if (_gyroMouse->isRunning()) {
        // Stop Logic
        _gyroMouse->stop();
        if (_macroManager->isGyroModeActive() && _macroManager->hasSavedGyroCombo()) {
            _macroManager->restoreSavedGyroCombo();
        }
        _macroManager->setGyroModeActive(false);
    } else {
        // Start Logic
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

void GyroMouseToggleCommand::release() {
    // No action on release
}
