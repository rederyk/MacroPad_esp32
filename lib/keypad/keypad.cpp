/*
 * ESP32 MacroPad Project
 * Copyright (C) [2025] [Enrico Mori]
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#include "keypad.h"

Keypad::Keypad(const KeypadConfig* config) : config(config) {
    // Initialize key state tracking arrays
    keyStates = new bool*[config->rows];
    lastKeyStates = new bool*[config->rows];
    lastKeyTime = new unsigned long*[config->rows];
    
    for (byte r = 0; r < config->rows; r++) {
        keyStates[r] = new bool[config->cols]();
        lastKeyStates[r] = new bool[config->cols]();
        lastKeyTime[r] = new unsigned long[config->cols]();
    }
}

Keypad::~Keypad() {
    // Clean up dynamically allocated arrays
    for (byte r = 0; r < config->rows; r++) {
        delete[] keyStates[r];
        delete[] lastKeyStates[r];
        delete[] lastKeyTime[r];
    }
    delete[] keyStates;
    delete[] lastKeyStates;
    delete[] lastKeyTime;
}

void Keypad::setup() {
    if (!config->invertDirection) {
        // Configure columns as outputs and rows as inputs
        for (byte c = 0; c < config->cols; c++) {
            pinMode(config->colPins[c], OUTPUT);
            digitalWrite(config->colPins[c], HIGH);
        }
        for (byte r = 0; r < config->rows; r++) {
            pinMode(config->rowPins[r], INPUT_PULLUP);
        }
    } else {
        // Configure rows as outputs and columns as inputs
        for (byte r = 0; r < config->rows; r++) {
            pinMode(config->rowPins[r], OUTPUT);
            digitalWrite(config->rowPins[r], HIGH);
        }
        for (byte c = 0; c < config->cols; c++) {
            pinMode(config->colPins[c], INPUT_PULLUP);
        }
    }
}

bool Keypad::processInput() {
    for (byte r = 0; r < config->rows; r++) {
        for (byte c = 0; c < config->cols; c++) {
            bool currentState;
            
            // Read key state based on diode direction
            if (!config->invertDirection) {
                digitalWrite(config->colPins[c], LOW);
                currentState = (digitalRead(config->rowPins[r]) == LOW);
                digitalWrite(config->colPins[c], HIGH);
            } else {
                digitalWrite(config->rowPins[r], LOW);
                currentState = (digitalRead(config->colPins[c]) == LOW);
                digitalWrite(config->rowPins[r], HIGH);
            }

            // Debounce check
            if (currentState != lastKeyStates[r][c] && 
                (millis() - lastKeyTime[r][c]) > KEY_DEBOUNCE_TIME) {
                
                lastKeyTime[r][c] = millis();
                lastKeyStates[r][c] = currentState;
                
                if (currentState != keyStates[r][c]) {
                    keyStates[r][c] = currentState;
                    
                    // Store key event
                    currentEvent.type = InputEvent::EventType::KEY_PRESS;
                    currentEvent.value1 = r * config->cols + c; // Unique key code
                    currentEvent.value2 = config->keys[r][c];   // Key character
                    currentEvent.state = currentState;
                    currentEvent.text = "";
                    hasEvent = true;
                    return true;
                }
            }
        }
    }
    return false;
}

InputEvent Keypad::getEvent() {
    hasEvent = false;
    return currentEvent;
}
