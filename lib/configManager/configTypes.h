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

#ifndef CONFIG_TYPES_H
#define CONFIG_TYPES_H

#include <Arduino.h>
#include <vector>

struct KeypadConfig
{
    byte rows;
    byte cols;
    std::vector<byte> rowPins;
    std::vector<byte> colPins;
    std::vector<std::vector<char>> keys;
    bool invertDirection;
};

struct EncoderConfig
{
    byte pinA;
    byte pinB;
    byte buttonPin;
    int stepValue;
};
struct LedConfig
{
    byte pinRed;
    byte pinGreen;
    byte pinBlue;
    bool anodeCommon;
    bool active;
};

struct AccelerometerConfig
{

    byte sdaPin;
    byte sclPin;
    float sensitivity;
    int sampleRate;
    int threshold;
    String axisMap;
    bool active;
    String type;
    uint8_t address;
};

struct WifiConfig
{
    String ap_ssid;
    String ap_password;
    String router_ssid;
    String router_password;
};

struct SystemConfig
{
    bool ap_autostart;
    bool router_autostart;
    bool enable_BLE;
    bool serial_enabled;
    int BleMacAdd;
    int combo_timeout;
    String BleName;
    // Nuovi campi per la gestione del power
    bool sleep_enabled;             // Abilitare il sleep mode
    unsigned long sleep_timeout_ms; // Timeout di inattività in millisecondi
    gpio_num_t wakeup_pin;                 // Pin GPIO per il wakeup
};

#endif
