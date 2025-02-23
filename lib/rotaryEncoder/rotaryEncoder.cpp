#include "rotaryEncoder.h"

// State transition table for encoder direction detection
// Rows: lastState, Columns: currentState
// Values: -1 = CCW, 0 = invalid/no change, 1 = CW
const int8_t RotaryEncoder::stateTransitionTable[4][4] = {
    { 0,  1, -1,  0}, // Last state 00
    {-1,  0,  0,  1}, // Last state 01
    { 1,  0,  0, -1}, // Last state 10
    { 0, -1,  1,  0}  // Last state 11
};

RotaryEncoder::RotaryEncoder(const EncoderConfig* config) : config(config) {}

void RotaryEncoder::setup() {
    // Configure pins with explicit pullup resistors
    pinMode(config->pinA, INPUT_PULLUP);
    pinMode(config->pinB, INPUT_PULLUP);
    pinMode(config->buttonPin, INPUT_PULLUP);
    
    // Get initial state
    lastState = (digitalRead(config->pinA) << 1) | digitalRead(config->pinB);
}

bool RotaryEncoder::readEncoder(int& direction) {
    // Read current state only if needed
    bool currentPinA = digitalRead(config->pinA);
    bool currentPinB = digitalRead(config->pinB);
    
    // Only process if either pin changed
    if (currentPinA != lastPinAState || currentPinB != lastPinBState) {
        uint8_t currentState = (currentPinA << 1) | currentPinB;
        direction = 0;
        
        // Update cached pin states
        lastPinAState = currentPinA;
        lastPinBState = currentPinB;
        
        // Only process if state changed
        if (currentState != lastState) {
            // Use state transition table to detect direction
            int8_t transition = stateTransitionTable[lastState][currentState];
            
            if (transition != 0) {
                rotaryCounter++;
                if (rotaryCounter == 4) {  // Complete cycle
                    direction = transition;
                    rotaryCounter = 0;
                }
            } else {
                // Invalid state transition, reset counter
                rotaryCounter = 0;
            }
            
            lastState = currentState;
            return (direction != 0);
        }
    }
    return false;
}

bool RotaryEncoder::processInput() {
    bool hasNewEvent = false;
    unsigned long currentTime = millis();
    
    // Handle release timing
    if (waitingForRelease && (currentTime - lastRotationTime >= 50)) {
        currentEvent.type = InputEvent::EventType::ROTATION;
        currentEvent.value1 = 0;
        currentEvent.value2 = encoderValue;
        currentEvent.state = false;
        waitingForRelease = false;
        hasNewEvent = true;
    }
    
    // Handle encoder rotation
    int direction = 0;
    if (readEncoder(direction)) {
        currentEvent.type = InputEvent::EventType::ROTATION;
        currentEvent.value1 = direction * config->stepValue;
        currentEvent.value2 = encoderValue + (direction * config->stepValue);
        currentEvent.state = true;
        encoderValue += direction * config->stepValue;
        lastRotationTime = currentTime;
        waitingForRelease = true;
        hasNewEvent = true;
    }

    // Handle encoder button
    int reading = digitalRead(config->buttonPin);
    if (reading != lastButtonState) {
        currentEvent.type = InputEvent::EventType::BUTTON;
        currentEvent.value1 = 0;
        currentEvent.value2 = 0;
        currentEvent.state = (reading == LOW);
        lastButtonState = reading;
        hasNewEvent = true;
    }

    return hasNewEvent;
}

InputEvent RotaryEncoder::getEvent() {
    return currentEvent;
}

int RotaryEncoder::getEncoderValue() {
    return encoderValue;
}

void RotaryEncoder::resetEncoderValue() {
    encoderValue = 0;
}
