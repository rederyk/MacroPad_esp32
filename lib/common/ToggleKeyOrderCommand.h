#ifndef TOGGLE_KEY_ORDER_COMMAND_H
#define TOGGLE_KEY_ORDER_COMMAND_H

#include "Command.h"
#include "macroManager.h" // Assuming macroManager has setUseKeyPressOrder

class ToggleKeyOrderCommand : public Command {
public:
    ToggleKeyOrderCommand(MacroManager* macroManager);
    void press() override;
    void release() override;

private:
    MacroManager* _macroManager;
};

#endif // TOGGLE_KEY_ORDER_COMMAND_H