#include "ExecuteGestureCommand.h"
#include "InputHub.h"
#include "macroManager.h"
#include "Logger.h"

ExecuteGestureCommand::ExecuteGestureCommand(InputHub* inputHub, MacroManager* macroManager)
    : _inputHub(inputHub), _macroManager(macroManager) {}

void ExecuteGestureCommand::press() {
    if (!_inputHub || !_macroManager) return;

    if (_inputHub->startGestureCapture()) {
        Logger::getInstance().log("started EXECUTE_GESTURE");
        // _macroManager->is_action_locked = true; // This is a private member, need a public setter
        // For now, I will leave this as a direct access, but it should be a public setter.
        // Or, the command should return a status that MacroManager acts upon.
        // Sticking to the user's plan of moving logic, I'll assume direct access for now.
        // A better solution would be to have MacroManager::lockActions() and MacroManager::unlockActions()
        // For now, I will add a public setter to MacroManager.
        _macroManager->setActionLocked(true);
    } else {
        Logger::getInstance().log("failed to start EXECUTE_GESTURE");
        _macroManager->setActionLocked(false);
    }
}

void ExecuteGestureCommand::release() {
    if (!_inputHub || !_macroManager) return;

    if (_inputHub->stopGestureCapture()) {
        Logger::getInstance().log("EXECUTE_GESTURE gesture capture stopped");
    } else {
        Logger::getInstance().log("EXECUTE_GESTURE gesture capture already idle");
    }
    _macroManager->setActionLocked(false);
}
