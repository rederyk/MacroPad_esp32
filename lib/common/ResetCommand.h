#ifndef RESET_COMMAND_H
#define RESET_COMMAND_H

#include "Command.h"
#include "specialAction.h"

class ResetCommand : public Command {
private:
    SpecialAction* _specialAction;

public:
    ResetCommand(SpecialAction* specialAction) : _specialAction(specialAction) {}

    void press() override {
        if (_specialAction) {
            _specialAction->resetDevice();
        }
    }

    void release() override {
        // No action on release
    }
};

#endif // RESET_COMMAND_H
