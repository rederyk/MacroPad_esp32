#ifndef KEYPAD_H
#define KEYPAD_H

#include <Arduino.h>
#include "inputDevice.h"
#include "configManager.h"

class Keypad : public InputDevice {
private:
    InputEvent currentEvent;
    bool hasEvent = false;
    const KeypadConfig* config;
    
    // Key state tracking
    bool** keyStates;
    bool** lastKeyStates;
    unsigned long** lastKeyTime;
    static const unsigned long KEY_DEBOUNCE_TIME = 10;

public:
    Keypad(const KeypadConfig* config);
    ~Keypad();
    void setup() override;
    bool processInput() override;
    InputEvent getEvent() override;
};

#endif
