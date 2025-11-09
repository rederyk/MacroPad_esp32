#ifndef DELAY_COMMAND_H
#define DELAY_COMMAND_H

#include "Command.h"

class SpecialAction;

class DelayCommand : public Command {
public:
    DelayCommand(SpecialAction* specialAction, int delayMs);
    void press() override;
    void release() override;

private:
    SpecialAction* _specialAction;
    int _delayMs;
};

#endif // DELAY_COMMAND_H
