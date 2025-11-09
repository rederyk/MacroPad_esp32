#include "LedCommand.h"

LedCommand::LedCommand(SpecialAction* specialAction, const std::string& actionString)
    : _specialAction(specialAction), _actionString(actionString) {}

void LedCommand::press() {
    if (!_specialAction) return;

    if (_actionString.rfind("LED_RGB_", 0) == 0) {
        handleLedRgbCommand();
    } else if (_actionString == "LED_OFF") {
        handleLedOffCommand();
    } else if (_actionString == "LED_SAVE") {
        handleLedSaveCommand();
    } else if (_actionString == "LED_RESTORE") {
        handleLedRestoreCommand();
    } else if (_actionString == "LED_INFO") {
        handleLedInfoCommand();
    }
}

void LedCommand::release() {
    // No specific release action for LED commands
}

void LedCommand::handleLedRgbCommand() {
    // Extract parameters after "LED_RGB_"
    std::string params = _actionString.substr(8); // Skip "LED_RGB_"

    // Parse the three color components separated by underscores
    std::vector<std::string> components;
    size_t start = 0;
    size_t end = params.find('_');

    // Split by underscores
    while (end != std::string::npos)
    {
        components.push_back(params.substr(start, end - start));
        start = end + 1;
        end = params.find('_', start);
    }
    // Add last component
    components.push_back(params.substr(start));

    if (components.size() == 3)
    {
        int values[3] = {0, 0, 0};           // Absolute values
        int deltas[3] = {0, 0, 0};           // Relative adjustments
        bool hasRelative = false;
        bool hasAbsolute = false;

        // Parse each component
        for (int i = 0; i < 3; i++)
        {
            std::string comp = components[i];

            // Check for PLUS/MINUS modifiers
            if (comp == "PLUS_PLUS")
            {
                deltas[i] = _specialAction->ledAdjustmentStep * 2;
                hasRelative = true;
            }
            else if (comp == "PLUS")
            {
                deltas[i] = _specialAction->ledAdjustmentStep;
                hasRelative = true;
            }
            else if (comp == "MINUS_MINUS")
            {
                deltas[i] = -_specialAction->ledAdjustmentStep * 2;
                hasRelative = true;
            }
            else if (comp == "MINUS")
            {
                deltas[i] = -_specialAction->ledAdjustmentStep;
                hasRelative = true;
            }
            else
            {
                // Try to parse as number (absolute value)
                try
                {
                    values[i] = std::stoi(comp);
                    hasAbsolute = true;
                }
                catch (const std::exception &e)
                {
                    Logger::getInstance().log("Invalid LED component: " + String(comp.c_str()));
                    return;
                }
            }
        }

        // Execute the command based on what we parsed
        if (hasRelative && !hasAbsolute)
        {
            // Pure relative adjustment
            _specialAction->adjustLedColor(deltas[0], deltas[1], deltas[2]);
        }
        else if (hasAbsolute && !hasRelative)
        {
            // Pure absolute set
            _specialAction->setLedColor(values[0], values[1], values[2], false);
        }
        else if (hasAbsolute && hasRelative)
        {
            // Mixed: first get current color, apply deltas, then set absolutes
            int currentRed, currentGreen, currentBlue;
            Led::getInstance().getColor(currentRed, currentGreen, currentBlue);

            // Start with current values
            int finalRed = currentRed;
            int finalGreen = currentGreen;
            int finalBlue = currentBlue;

            // Apply deltas where specified
            if (deltas[0] != 0) finalRed += deltas[0];
            else if (values[0] != 0 || components[0] == "0") finalRed = values[0];

            if (deltas[1] != 0) finalGreen += deltas[1];
            else if (values[1] != 0 || components[1] == "0") finalGreen = values[1];

            if (deltas[2] != 0) finalBlue += deltas[2];
            else if (values[2] != 0 || components[2] == "0") finalBlue = values[2];

            _specialAction->setLedColor(finalRed, finalGreen, finalBlue, false);
        }
    }
    else
    {
        Logger::getInstance().log("Invalid LED_RGB format: expected 3 components");
    }
}

void LedCommand::handleLedOffCommand() {
    _specialAction->turnOffLed();
}

void LedCommand::handleLedSaveCommand() {
    _specialAction->saveLedColor();
}

void LedCommand::handleLedRestoreCommand() {
    _specialAction->restoreLedColor();
}

void LedCommand::handleLedInfoCommand() {
    _specialAction->showLedInfo();
}
