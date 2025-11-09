#include "SwitchComboCommand.h"
#include "Logger.h"
#include <stdexcept> // For std::stoi exceptions

SwitchComboCommand::SwitchComboCommand(MacroManager* macroManager, const std::string& actionString)
    : _macroManager(macroManager), _actionString(actionString), _setNumber(0) {
    
    if (actionString.rfind("SWITCH_MY_COMBO_", 0) == 0) {
        _prefix = "my_combo";
        std::string numStr = actionString.substr(16); // After "SWITCH_MY_COMBO_"
        try {
            _setNumber = std::stoi(numStr);
        } catch (const std::exception& e) {
            Logger::getInstance().log("Invalid SWITCH_MY_COMBO format: " + String(actionString.c_str()));
            _setNumber = -1; // Indicate error
        }
    } else if (actionString.rfind("SWITCH_COMBO_", 0) == 0) {
        _prefix = "combo";
        std::string numStr = actionString.substr(13); // After "SWITCH_COMBO_"
        try {
            _setNumber = std::stoi(numStr);
        } catch (const std::exception& e) {
            Logger::getInstance().log("Invalid SWITCH_COMBO format: " + String(actionString.c_str()));
            _setNumber = -1; // Indicate error
        }
    } else {
        Logger::getInstance().log("SwitchComboCommand: Unknown action string format: " + String(actionString.c_str()));
        _setNumber = -1; // Indicate error
    }
}

void SwitchComboCommand::press() {
    // No action on press for this command
    Logger::getInstance().log("SwitchComboCommand: Press (no action)");
}

void SwitchComboCommand::release() {
    if (_macroManager && _setNumber != -1) {
        Logger::getInstance().log("Switch to " + String(_prefix.c_str()) + "_" + String(_setNumber) + " requested");
        _macroManager->setPendingComboSwitch(_prefix, _setNumber);

        if (_macroManager->isGyroModeActive()) {
            _macroManager->saveCurrentComboForGyro(); // This will save the pending combo
        }
    } else {
        Logger::getInstance().log("SwitchComboCommand: MacroManager is null or invalid set number");
    }
}