#ifndef FLASHLIGHT_COMMAND_H
#define FLASHLIGHT_COMMAND_H

#include "Command.h"

class SpecialAction;

class FlashlightCommand : public Command {
public:
    FlashlightCommand(SpecialAction* specialAction);
    void press() override;
    void release() override;

private:
    SpecialAction* _specialAction;
};

#endif // FLASHLIGHT_COMMAND_H
