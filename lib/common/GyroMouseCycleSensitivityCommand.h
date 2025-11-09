#ifndef GYRO_MOUSE_CYCLE_SENSITIVITY_COMMAND_H
#define GYRO_MOUSE_CYCLE_SENSITIVITY_COMMAND_H

#include "Command.h"

class GyroMouse;

class GyroMouseCycleSensitivityCommand : public Command {
public:
    GyroMouseCycleSensitivityCommand(GyroMouse* gyroMouse);
    void press() override;
    void release() override;

private:
    GyroMouse* _gyroMouse;
};

#endif // GYRO_MOUSE_CYCLE_SENSITIVITY_COMMAND_H
