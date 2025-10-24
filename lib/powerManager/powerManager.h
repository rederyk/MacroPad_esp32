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


#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include "Logger.h"
#include "configManager.h"
#include <vector>
#include <driver/gpio.h>

class PowerManager
{
private:
    unsigned long lastActivityTime;
    unsigned long inactivityTimeout; // in milliseconds
    bool sleepEnabled;
    bool isBleMode;
    gpio_num_t wakeupPin;
    gpio_num_t fallbackWakePin;
    bool fallbackWakePinValid;

public:
    PowerManager();

    void begin(const SystemConfig &sysConfig, const KeypadConfig &keypadConfig, const EncoderConfig &encoderConfig);
    void resetActivityTimer();
    void registerActivity();
    bool checkInactivity();
    void enterDeepSleep(bool force = false);
};

#endif // POWER_MANAGER_H
