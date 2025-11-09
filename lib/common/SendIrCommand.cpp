#include "SendIrCommand.h"
#include "specialAction.h"
#include "macroManager.h"
#include "Logger.h"
#include <vector>

SendIrCommand::SendIrCommand(SpecialAction* specialAction, MacroManager* macroManager, const std::string& actionString)
    : _specialAction(specialAction), _macroManager(macroManager), _actionString(actionString) {}

void SendIrCommand::press() {
    if (!_specialAction || !_macroManager) return;
    parseAndSendIrCommand();
}

void SendIrCommand::release() {
    // No specific release action for SEND_IR commands
}

void SendIrCommand::parseAndSendIrCommand() {
    // Extract the part after "SEND_IR_" (8 characters)
    std::string remainder = _actionString.substr(8);

    // 1. Check for interactive mode: SEND_IR_DEV_<deviceId>
    if (remainder.rfind("DEV_", 0) == 0)
    {
        // Interactive send mode
        std::string devStr = remainder.substr(4); // After "DEV_"
        try
        {
            int deviceId = std::stoi(devStr);
            String exitCombo = String(_macroManager->getCurrentActivationCombo().c_str());
            _specialAction->toggleSendIR(deviceId, exitCombo);
            _macroManager->clearActiveKeys();
        }
        catch (const std::exception &e)
        {
            Logger::getInstance().log("Invalid SEND_IR_DEV format: " + String(_actionString.c_str()));
        }
    }
    // 2. Check for numeric direct send: SEND_IR_CMD_<deviceId>_CMD<commandId>
    else if (remainder.rfind("CMD_", 0) == 0)
    {
        std::string numericPart = remainder.substr(4); // After "CMD_"
        size_t cmdPos = numericPart.find("_CMD");

        if (cmdPos != std::string::npos)
        {
            std::string devStr = numericPart.substr(0, cmdPos);
            std::string cmdStr = numericPart.substr(cmdPos + 4);

            try
            {
                int deviceId = std::stoi(devStr);
                int commandId = std::stoi(cmdStr);
                String deviceName = String("dev") + String(deviceId);
                String commandName = String("cmd") + String(commandId);
                _specialAction->sendIRCommand(deviceName, commandName);
            }
            catch (const std::exception &e)
            {
                Logger::getInstance().log("Invalid SEND_IR_CMD format: " + String(_actionString.c_str()));
            }
        }
    }
    // 3. Descriptive direct send: SEND_IR_<deviceName>_<commandName>
    else
    {
        size_t underscorePos = remainder.find('_');
        if (underscorePos != std::string::npos)
        {
            String deviceName = String(remainder.substr(0, underscorePos).c_str());
            String commandName = String(remainder.substr(underscorePos + 1).c_str());
            _specialAction->sendIRCommand(deviceName, commandName);
        }
        else
        {
            Logger::getInstance().log("Invalid SEND_IR format (missing underscore): " + String(_actionString.c_str()));
        }
    }
}
