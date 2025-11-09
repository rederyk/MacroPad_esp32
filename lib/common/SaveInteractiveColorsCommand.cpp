#include "SaveInteractiveColorsCommand.h"
#include "Logger.h"

SaveInteractiveColorsCommand::SaveInteractiveColorsCommand(InputHub* inputHub)
    : _inputHub(inputHub) {}

void SaveInteractiveColorsCommand::press() {
    // No action on press for this command
    Logger::getInstance().log("SaveInteractiveColorsCommand: Press (no action)");
}

void SaveInteractiveColorsCommand::release() {
    if (_inputHub) {
        _inputHub->saveReactiveLightingColors();
        Logger::getInstance().log("SaveInteractiveColorsCommand: Saved interactive colors");
    } else {
        Logger::getInstance().log("SaveInteractiveColorsCommand: InputHub is null");
    }
}