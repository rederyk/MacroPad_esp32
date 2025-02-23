#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H

#include <Arduino.h>
#include "inputDevice.h"
#include "configManager.h"

class RotaryEncoder : public InputDevice {
private:
    InputEvent currentEvent;
    bool hasEvent = false;
    const EncoderConfig* config;
    
    // Encoder state
    volatile int encoderValue = 0;
    volatile bool buttonPressed = false;
    int lastEncoderValue = 0;
    bool lastButtonState = false;
    
    // State tracking
    uint8_t lastState = 0;
    uint8_t rotaryCounter = 0;
    bool lastPinAState = false;
    bool lastPinBState = false;
    
    // Release timing
    unsigned long lastRotationTime = 0;
    bool waitingForRelease = false;
    
    // State transition table for encoder
    static const int8_t stateTransitionTable[4][4];

    bool readEncoder(int& direction);

public:
    RotaryEncoder(const EncoderConfig* config);
    void setup() override;
    bool processInput() override;
    InputEvent getEvent() override;
    int getEncoderValue();
    void resetEncoderValue();
};

#endif
