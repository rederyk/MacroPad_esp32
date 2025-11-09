#ifndef SAVE_INTERACTIVE_COLORS_COMMAND_H
#define SAVE_INTERACTIVE_COLORS_COMMAND_H

#include "Command.h"
#include "InputHub.h" // Assuming InputHub has saveReactiveLightingColors

class SaveInteractiveColorsCommand : public Command {
public:
    SaveInteractiveColorsCommand(InputHub* inputHub);
    void press() override;
    void release() override;

private:
    InputHub* _inputHub;
};

#endif // SAVE_INTERACTIVE_COLORS_COMMAND_H