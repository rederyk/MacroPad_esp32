#include "ToggleReactiveLightingCommand.h"
#include "Logger.h"

ToggleReactiveLightingCommand::ToggleReactiveLightingCommand(InputHub* inputHub)
    : _inputHub(inputHub) {}

void ToggleReactiveLightingCommand::press() {
    // No action on press for this command
    Logger::getInstance().log("ToggleReactiveLightingCommand: Press (no action)");
}

void ToggleReactiveLightingCommand::release() {
    if (_inputHub) {
        const bool newState = !_inputHub->isReactiveLightingEnabled();
        _inputHub->setReactiveLightingEnabled(newState);
        Logger::getInstance().log("ToggleReactiveLightingCommand: Toggled reactive lighting to " + String(newState));
    } else {
        Logger::getInstance().log("ToggleReactiveLightingCommand: InputHub is null");
    }
}