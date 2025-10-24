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
