#ifndef INPUT_DEVICE_H
#define INPUT_DEVICE_H

#include <Arduino.h>

struct InputEvent {
    enum class EventType : uint8_t {
        KEY_PRESS,
        ROTATION,
        BUTTON,
        MOTION
    };
    
    EventType type;
    int value1;  // For key codes, rotation direction, etc.
    int value2;  // For additional data (e.g., acceleration values)
    bool state;  // Pressed/released, button state, etc.
};

class InputDevice {
public:
    virtual void setup() = 0;
    virtual bool processInput() = 0;
    virtual InputEvent getEvent() = 0;
};

#endif
