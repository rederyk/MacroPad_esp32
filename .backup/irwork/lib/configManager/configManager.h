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


#ifndef CONFIGURATION_MANAGER_H
#define CONFIGURATION_MANAGER_H

#include <Arduino.h>
#include <map>
#include <vector>
#include <string>
#include <ArduinoJson.h>

#include "configTypes.h"

// Forward declaration
class MacroManager;

class ConfigurationManager
{
private:
    KeypadConfig keypadConfig;
    EncoderConfig encoderConfig;
    LedConfig ledConfig;
    IRSensorConfig irSensorConfig;
    IRLedConfig irLedConfig;
    AccelerometerConfig accelerometerConfig;
    SystemConfig systemConfig;

public:
    ConfigurationManager();
    bool loadConfig();
    const KeypadConfig &getKeypadConfig() const;
    const EncoderConfig &getEncoderConfig() const;
    const AccelerometerConfig &getAccelerometerConfig() const;
    const WifiConfig &getWifiConfig() const;
    const SystemConfig &getSystemConfig() const;
    const LedConfig &getLedConfig() const;
    const IRSensorConfig &getIrSensorConfig() const;
    const IRLedConfig &getIrLedConfig() const;

private:
    WifiConfig wifiConfig;
};

#endif
