#ifndef GYRO_MOUSE_START_COMMAND_H
#define GYRO_MOUSE_START_COMMAND_H

#include "Command.h"

class GyroMouse;
class MacroManager;

class GyroMouseStartCommand : public Command {
public:
    GyroMouseStartCommand(GyroMouse* gyroMouse, MacroManager* macroManager);
    void press() override;
    void release() override;

private:
    GyroMouse* _gyroMouse;
    MacroManager* _macroManager;
};

#endif // GYRO_MOUSE_START_COMMAND_H
