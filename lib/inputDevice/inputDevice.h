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
