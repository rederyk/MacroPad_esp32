#ifndef MEM_INFO_COMMAND_H
#define MEM_INFO_COMMAND_H

#include "Command.h"
#include "specialAction.h"

class MemInfoCommand : public Command {
private:
    SpecialAction* _specialAction;

public:
    MemInfoCommand(SpecialAction* specialAction) : _specialAction(specialAction) {}

    void press() override {
        if (_specialAction) {
            _specialAction->printMemoryInfo();
        }
    }

    void release() override {
        // No action on release
    }
};

#endif // MEM_INFO_COMMAND_H
