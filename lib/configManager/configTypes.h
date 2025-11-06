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
    uint8_t brightness = 255;
};

struct IRSensorConfig
{
    int pin;
    bool active;
};

struct IRLedConfig
{
    int pin;
    bool anodeGpio;
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
    String axisDir;
    bool active;
    String type;
    uint8_t address;
    bool motionWakeEnabled;
    uint8_t motionWakeThreshold;
    uint8_t motionWakeDuration;
    uint8_t motionWakeHighPass;
    uint8_t motionWakeCycleRate;
    String gestureMode; // "auto", "mpu6050", "adxl345", "shape", "orientation"
};

struct SensitivitySettings
{
    String name;
    float scale;
    float deadzone;
    String mode;
    float gyroScale;
    float tiltScale;
    float tiltDeadzone;
    float hybridBlend;
    float accelerationCurve = 1.0f; // Acceleration curve exponent: <1.0 = sub-linear (precision), 1.0 = linear, >1.0 = super-linear (speed)
    int8_t invertXOverride = -1; // -1 = inherit global, 0 = false, 1 = true
    int8_t invertYOverride = -1;
    int8_t swapAxesOverride = -1;
};

struct GyroMouseConfig
{
    bool enabled;
    float smoothing;
    bool invertX;
    bool invertY;
    bool swapAxes;
    uint8_t defaultSensitivity;
    float orientationAlpha;
    float tiltLimitDegrees;
    float tiltDeadzoneDegrees;
    float recenterRate;
    float recenterThresholdDegrees;
    bool absoluteRecenter;
    int32_t absoluteRangeX;
    int32_t absoluteRangeY;
    float clickSlowdownFactor; // Slowdown factor when mouse button is pressed (0.0-1.0)
    std::vector<SensitivitySettings> sensitivities;
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
    unsigned long sleep_timeout_mouse_ms; // Timeout dedicato per modalità mouse
    unsigned long sleep_timeout_ir_ms;    // Timeout dedicato per modalità IR
    gpio_num_t wakeup_pin;                 // Pin GPIO per il wakeup
};
// TODO add config for ir from json in all class
#endif
