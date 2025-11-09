#ifndef CALIBRATE_SENSOR_COMMAND_H
#define CALIBRATE_SENSOR_COMMAND_H

#include "Command.h"
#include "specialAction.h"

class CalibrateSensorCommand : public Command {
private:
    SpecialAction* _specialAction;

public:
    CalibrateSensorCommand(SpecialAction* specialAction) : _specialAction(specialAction) {}

    void press() override {
        if (_specialAction) {
            _specialAction->calibrateSensor();
        }
    }

    void release() override {
        // No action on release
    }
};

#endif // CALIBRATE_SENSOR_COMMAND_H
