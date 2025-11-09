#include "ToggleKeyOrderCommand.h"
#include "Logger.h"

ToggleKeyOrderCommand::ToggleKeyOrderCommand(MacroManager* macroManager)
    : _macroManager(macroManager) {}

void ToggleKeyOrderCommand::press() {
    // No action on press for this command
    Logger::getInstance().log("ToggleKeyOrderCommand: Press (no action)");
}

void ToggleKeyOrderCommand::release() {
    if (_macroManager) {
        bool currentUseKeyPressOrder = _macroManager->getUseKeyPressOrder();
        _macroManager->setUseKeyPressOrder(!currentUseKeyPressOrder);
        Logger::getInstance().log("ToggleKeyOrderCommand: Toggled key press order to " + String(!currentUseKeyPressOrder));
    } else {
        Logger::getInstance().log("ToggleKeyOrderCommand: MacroManager is null");
    }
}