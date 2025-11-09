#ifndef SEND_IR_COMMAND_H
#define SEND_IR_COMMAND_H

#include "Command.h"
#include "specialAction.h"
#include "macroManager.h"
#include <string>

class SendIrCommand : public Command {
public:
    SendIrCommand(SpecialAction* specialAction, MacroManager* macroManager, const std::string& actionString);
    void press() override;
    void release() override;

private:
    SpecialAction* _specialAction;
    MacroManager* _macroManager;
    std::string _actionString;

    // Helper to parse and send IR commands
    void parseAndSendIrCommand();
};

#endif // SEND_IR_COMMAND_H
