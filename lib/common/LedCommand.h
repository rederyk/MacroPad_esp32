#ifndef LED_COMMAND_H
#define LED_COMMAND_H

#include "Command.h"
#include "specialAction.h"
#include "Led.h" // For Led::getInstance().getColor()
#include "Logger.h" // For logging
#include <string>
#include <vector>

class LedCommand : public Command {
public:
    LedCommand(SpecialAction* specialAction, const std::string& actionString);
    void press() override;
    void release() override;

private:
    SpecialAction* _specialAction;
    std::string _actionString;

    void handleLedRgbCommand();
    void handleLedOffCommand();
    void handleLedSaveCommand();
    void handleLedRestoreCommand();
    void handleLedInfoCommand();
};

#endif // LED_COMMAND_H
