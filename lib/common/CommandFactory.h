#ifndef COMMAND_FACTORY_H
#define COMMAND_FACTORY_H

#include <memory>
#include <string>
#include "Command.h"
#include "ScanIrDevCommand.h"
#include "SendIrCommand.h"
#include "LedCommand.h"
#include "LedBrightnessCommand.h"

// Forward declarations for all dependencies the factory might need
class SpecialAction;
class BLEController;
class GyroMouse;
class InputHub;
class WIFIManager;
class CombinationManager;
class MacroManager;

class CommandFactory {
public:
    CommandFactory(
        SpecialAction* specialAction,
        BLEController* bleController,
        GyroMouse* gyroMouse,
        InputHub* inputHub,
        WIFIManager* wifiManager,
        CombinationManager* comboManager
    );

    void setMacroManager(MacroManager* macroManager);

    std::unique_ptr<Command> create(const std::string& actionString);

private:
    SpecialAction* _specialAction;
    BLEController* _bleController;
    GyroMouse* _gyroMouse;
    InputHub* _inputHub;
    WIFIManager* _wifiManager;
    CombinationManager* _comboManager;
    MacroManager* _macroManager;
};

#endif // COMMAND_FACTORY_H