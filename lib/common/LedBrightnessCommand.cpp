#include "LedBrightnessCommand.h"

LedBrightnessCommand::LedBrightnessCommand(SpecialAction* specialAction, const std::string& actionString)
    : _specialAction(specialAction), _actionString(actionString) {}

void LedBrightnessCommand::press() {
    if (!_specialAction) return;
    handleLedBrightnessCommand();
}

void LedBrightnessCommand::release() {
    // No specific release action for LED brightness commands
}

void LedBrightnessCommand::handleLedBrightnessCommand() {
    // Extract parameter after "LED_BRIGHTNESS_"
    std::string param = _actionString.substr(15); // Skip "LED_BRIGHTNESS_"

    // Check for PLUS with optional multiplier (PLUS, PLUS2, PLUS3, etc.)
    if (param.rfind("PLUS", 0) == 0)
    {
        int multiplier = 1;
        if (param.length() > 4) // Has number after PLUS
        {
            try
            {
                std::string numStr = param.substr(4);
                multiplier = std::stoi(numStr);
            }
            catch (const std::exception &e)
            {
                Logger::getInstance().log("Invalid PLUS multiplier: " + String(param.c_str()));
                multiplier = 1;
            }
        }
        _specialAction->adjustBrightness(_specialAction->brightnessAdjustmentStep * multiplier);
    }
    // Check for MINUS with optional multiplier (MINUS, MINUS0, MINUS1, MINUS2, etc.)
    else if (param.rfind("MINUS", 0) == 0)
    {
        int multiplier = 1;
        if (param.length() > 5) // Has number after MINUS
        {
            try
            {
                std::string numStr = param.substr(5);
                int num = std::stoi(numStr);
                // MINUS0 or MINUS1 = same as MINUS (multiplier 1)
                multiplier = (num <= 1) ? 1 : num;
            }
            catch (const std::exception &e)
            {
                Logger::getInstance().log("Invalid MINUS multiplier: " + String(param.c_str()));
                multiplier = 1;
            }
        }
        _specialAction->adjustBrightness(-_specialAction->brightnessAdjustmentStep * multiplier);
    }
    else if (param == "INFO")
    {
        _specialAction->showBrightnessInfo();
    }
    else
    {
        // Try to parse as absolute value (0-255)
        try
        {
            int brightness = std::stoi(param);
            _specialAction->setBrightness(brightness);
        }
        catch (const std::exception &e)
        {
            Logger::getInstance().log("Invalid LED_BRIGHTNESS format: " + String(param.c_str()));
        }
    }
}
