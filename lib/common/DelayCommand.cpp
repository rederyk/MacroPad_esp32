#include "DelayCommand.h"
#include "specialAction.h"

DelayCommand::DelayCommand(SpecialAction* specialAction, int delayMs)
    : _specialAction(specialAction), _delayMs(delayMs) {}

void DelayCommand::press() {
    if (_specialAction) {
        _specialAction->actionDelay(_delayMs);
    }
}

void DelayCommand::release() {
    // No action on release
}
