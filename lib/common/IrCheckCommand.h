#ifndef IR_CHECK_COMMAND_H
#define IR_CHECK_COMMAND_H

#include "Command.h"
#include "specialAction.h"

class IrCheckCommand : public Command {
private:
    SpecialAction* _specialAction;

public:
    IrCheckCommand(SpecialAction* specialAction) : _specialAction(specialAction) {}

    void press() override {
        if (_specialAction) {
            _specialAction->checkIRSignal();
        }
    }

    void release() override {
        // No action on release
    }
};

#endif // IR_CHECK_COMMAND_H
