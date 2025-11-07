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
#include <time.h>

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
    bool sleep_enabled;                   // Abilitare il sleep mode
    unsigned long sleep_timeout_ms;       // Timeout di inattività in millisecondi
    unsigned long sleep_timeout_mouse_ms; // Timeout dedicato per modalità mouse
    unsigned long sleep_timeout_ir_ms;    // Timeout dedicato per modalità IR
    gpio_num_t wakeup_pin;                // Pin GPIO per il wakeup
};

enum class ScheduleTriggerType : uint8_t
{
    NONE = 0,
    TIME_OF_DAY,
    INTERVAL,
    ABSOLUTE_TIME,
    INPUT_EVENT
};

struct ScheduleTriggerConfig
{
    ScheduleTriggerType type{ScheduleTriggerType::NONE};
    uint32_t intervalMs{0};        // For interval triggers
    uint32_t jitterMs{0};          // Optional random jitter
    time_t absoluteEpoch{0};       // For absolute triggers
    uint8_t hour{0};               // For time-of-day triggers
    uint8_t minute{0};
    uint8_t second{0};
    uint8_t daysMask{0x7F};        // 7 bits -> Sun=0
    bool useUtc{false};            // Whether to ignore timezone offset
    String inputSource;            // For input/sensor triggers
    String inputType;              // e.g. KEY_PRESS/ROTATION/gesture id
    int inputValue{-1};
    int8_t inputState{-1};         // -1 = ignore, 0 = false, 1 = true
    String inputText;              // Optional textual match
};

struct ScheduledActionConfig
{
    String id;
    bool enabled{false};
    bool wakeFromSleep{false};
    bool preventSleep{false};
    bool runOnBoot{false};
    bool oneShot{false};
    bool allowOverlap{false};
    ScheduleTriggerConfig trigger;
    String actionType;
    String actionId;
    String actionParams; // Serialized JSON
    String description;
};

struct SchedulerConfig
{
    bool enabled{false};
    bool preventSleepIfPending{true};
    uint32_t sleepGuardSeconds{60};
    uint32_t wakeAheadSeconds{900};
    int timezoneOffsetMinutes{0};
    uint32_t pollIntervalMs{250};
    std::vector<ScheduledActionConfig> events;
};

#endif
