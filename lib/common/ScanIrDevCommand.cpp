#include "ScanIrDevCommand.h"
#include "specialAction.h"
#include "macroManager.h"

ScanIrDevCommand::ScanIrDevCommand(SpecialAction* specialAction, MacroManager* macroManager, int deviceId)
    : _specialAction(specialAction), _macroManager(macroManager), _deviceId(deviceId) {}

void ScanIrDevCommand::press() {
    if (!_specialAction || !_macroManager) return;

    // Pass the activation combo as exit combo
    String exitCombo = String(_macroManager->getCurrentActivationCombo().c_str());
    _specialAction->toggleScanIR(_deviceId, exitCombo);

    // Clean up state after IR mode exits
    _macroManager->clearActiveKeys();
}

void ScanIrDevCommand::release() {
    // No action on release
}
