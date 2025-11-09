#ifndef GYRO_MOUSE_RECENTER_COMMAND_H
#define GYRO_MOUSE_RECENTER_COMMAND_H

#include "Command.h"

class GyroMouse;

class GyroMouseRecenterCommand : public Command {
public:
    GyroMouseRecenterCommand(GyroMouse* gyroMouse);
    void press() override;
    void release() override;

private:
    GyroMouse* _gyroMouse;
};

#endif // GYRO_MOUSE_RECENTER_COMMAND_H
