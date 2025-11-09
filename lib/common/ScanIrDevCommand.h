#ifndef SCAN_IR_DEV_COMMAND_H
#define SCAN_IR_DEV_COMMAND_H

#include "Command.h"
#include <string>

class SpecialAction;
class MacroManager;

class ScanIrDevCommand : public Command {
public:
    ScanIrDevCommand(SpecialAction* specialAction, MacroManager* macroManager, int deviceId);
    void press() override;
    void release() override;

private:
    SpecialAction* _specialAction;
    MacroManager* _macroManager;
    int _deviceId;
};

#endif // SCAN_IR_DEV_COMMAND_H
