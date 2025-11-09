#ifndef EXECUTE_GESTURE_COMMAND_H
#define EXECUTE_GESTURE_COMMAND_H

#include "Command.h"
#include "Logger.h"

class InputHub;
class MacroManager;

class ExecuteGestureCommand : public Command {
public:
    ExecuteGestureCommand(InputHub* inputHub, MacroManager* macroManager);
    void press() override;
    void release() override;

private:
    InputHub* _inputHub;
    MacroManager* _macroManager;
};

#endif // EXECUTE_GESTURE_COMMAND_H
