#ifndef ENTER_SLEEP_COMMAND_H
#define ENTER_SLEEP_COMMAND_H

#include "Command.h"
#include "specialAction.h"

class EnterSleepCommand : public Command {
private:
    SpecialAction* _specialAction;

public:
    EnterSleepCommand(SpecialAction* specialAction) : _specialAction(specialAction) {}

    void press() override {
        if (_specialAction) {
            _specialAction->enterSleep();
        }
    }

    void release() override {
        // No action on release
    }
};

#endif // ENTER_SLEEP_COMMAND_H
