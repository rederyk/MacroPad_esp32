#ifndef LED_BRIGHTNESS_COMMAND_H
#define LED_BRIGHTNESS_COMMAND_H

#include "Command.h"
#include "specialAction.h"
#include "Logger.h" // For logging
#include <string>

class LedBrightnessCommand : public Command {
public:
    LedBrightnessCommand(SpecialAction* specialAction, const std::string& actionString);
    void press() override;
    void release() override;

private:
    SpecialAction* _specialAction;
    std::string _actionString;

    void handleLedBrightnessCommand();
};

#endif // LED_BRIGHTNESS_COMMAND_H
