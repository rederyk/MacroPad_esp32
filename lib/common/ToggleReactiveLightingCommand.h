#ifndef TOGGLE_REACTIVE_LIGHTING_COMMAND_H
#define TOGGLE_REACTIVE_LIGHTING_COMMAND_H

#include "Command.h"
#include "InputHub.h" // Assuming InputHub has isReactiveLightingEnabled and setReactiveLightingEnabled

class ToggleReactiveLightingCommand : public Command {
public:
    ToggleReactiveLightingCommand(InputHub* inputHub);
    void press() override;
    void release() override;

private:
    InputHub* _inputHub;
};

#endif // TOGGLE_REACTIVE_LIGHTING_COMMAND_H