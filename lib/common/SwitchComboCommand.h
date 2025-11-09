#ifndef SWITCH_COMBO_COMMAND_H
#define SWITCH_COMBO_COMMAND_H

#include "Command.h"
#include "macroManager.h" // Assuming MacroManager has setPendingComboSwitch, setGyroModeActive, isGyroModeActive, etc.

class SwitchComboCommand : public Command {
public:
    SwitchComboCommand(MacroManager* macroManager, const std::string& actionString);
    void press() override;
    void release() override;

private:
    MacroManager* _macroManager;
    std::string _actionString;
    std::string _prefix;
    int _setNumber;
};

#endif // SWITCH_COMBO_COMMAND_H