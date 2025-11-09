#include "FlashlightCommand.h"
#include "specialAction.h"

FlashlightCommand::FlashlightCommand(SpecialAction* specialAction)
    : _specialAction(specialAction) {}

void FlashlightCommand::press() {
    if (_specialAction) {
        _specialAction->toggleFlashlight();
    }
}

void FlashlightCommand::release() {
    // No action on release
}
