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

#include "configManager.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include <algorithm>
#include "Logger.h"
#include "FileSystemManager.h"

namespace
{
int dayNameToIndex(String name)
{
    name.toLowerCase();
    name.trim();
    if (name == "sun" || name == "sunday" || name == "dom" || name == "domenica" || name == "0")
        return 0;
    if (name == "mon" || name == "monday" || name == "lun" || name == "lunedi" || name == "1")
        return 1;
    if (name == "tue" || name == "tuesday" || name == "mar" || name == "martedi" || name == "2")
        return 2;
    if (name == "wed" || name == "wednesday" || name == "mer" || name == "mercoledi" || name == "3")
        return 3;
    if (name == "thu" || name == "thursday" || name == "gio" || name == "giovedi" || name == "4")
        return 4;
    if (name == "fri" || name == "friday" || name == "ven" || name == "venerdi" || name == "5")
        return 5;
    if (name == "sat" || name == "saturday" || name == "sab" || name == "sabato" || name == "6")
        return 6;
    if (name == "all" || name == "daily" || name == "*" || name == "everyday")
        return 7;
    if (name == "weekdays")
        return 8;
    if (name == "weekend")
        return 9;
    return -1;
}

void addDayTokenMask(const String &token, uint8_t &mask)
{
    int idx = dayNameToIndex(token);
    if (idx >= 0 && idx < 7)
    {
        mask |= (1 << idx);
    }
    else if (idx == 7)
    {
        mask = 0x7F;
    }
    else if (idx == 8)
    {
        mask |= (0b0111110); // Monday-Friday bits 1-5
    }
    else if (idx == 9)
    {
        mask |= (1 << 0) | (1 << 6);
    }
}

uint8_t parseDaysMask(JsonVariantConst variant)
{
    uint8_t mask = 0;

    if (variant.is<JsonArrayConst>())
    {
        for (JsonVariantConst entry : variant.as<JsonArrayConst>())
        {
            if (entry.is<int>())
            {
                int idx = entry.as<int>();
                if (idx >= 0 && idx < 7)
                {
                    mask |= (1 << idx);
                }
            }
            else if (entry.is<const char *>())
            {
                addDayTokenMask(String(entry.as<const char *>()), mask);
            }
        }
    }
    else if (variant.is<const char *>())
    {
        String text = variant.as<const char *>();
        int start = 0;
        while (start < text.length())
        {
            int end = text.indexOf(',', start);
            if (end == -1)
            {
                end = text.length();
            }
            String token = text.substring(start, end);
            token.trim();
            if (token.length() > 0)
            {
                addDayTokenMask(token, mask);
            }
            start = end + 1;
        }
    }
    else if (variant.is<int>())
    {
        int idx = variant.as<int>();
        if (idx >= 0 && idx < 7)
        {
            mask |= (1 << idx);
        }
    }

    if (mask == 0)
    {
        mask = 0x7F;
    }
    return mask;
}

ScheduleTriggerType parseTriggerType(const String &typeStr)
{
    String lowered = typeStr;
    lowered.toLowerCase();
    lowered.trim();
    if (lowered == "time" || lowered == "time_of_day" || lowered == "daily" || lowered == "clock")
    {
        return ScheduleTriggerType::TIME_OF_DAY;
    }
    if (lowered == "interval" || lowered == "every" || lowered == "loop")
    {
        return ScheduleTriggerType::INTERVAL;
    }
    if (lowered == "absolute" || lowered == "once" || lowered == "epoch")
    {
        return ScheduleTriggerType::ABSOLUTE_TIME;
    }
    if (lowered == "input" || lowered == "sensor" || lowered == "event")
    {
        return ScheduleTriggerType::INPUT_EVENT;
    }
    return ScheduleTriggerType::NONE;
}
} // namespace

ConfigurationManager::ConfigurationManager() : systemConfig() {}

bool ConfigurationManager::loadConfig()
{
    if (!FileSystemManager::ensureMounted())
    {
        Logger::getInstance().log("Failed to mount LittleFS");
        return false;
    }

    File configFile = LittleFS.open("/config.json");
    if (!configFile)
    {
        Logger::getInstance().log("Failed to open config file");
        return false;
    }

    // Create a filter to only parse the sections we care about
    StaticJsonDocument<256> filter;
    filter["wifi"] = true;
    filter["keypad"] = true;
    filter["encoder"] = true;
    filter["accelerometer"] = true;
    filter["system"] = true;
    filter["led"] = true;
    filter["irSensor"] = true;
    filter["irLed"] = true;
    filter["gyromouse"] = true;
    filter["scheduler"] = true;

    // Increase buffer size and add error handling
    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, configFile, DeserializationOption::Filter(filter));
    if (error)
    {
        Logger::getInstance().log("Failed to parse config file: " + String(error.c_str()));
        configFile.close();
        return false;
    }

    // Clear any existing data
    keypadConfig = KeypadConfig();
    encoderConfig = EncoderConfig();
    accelerometerConfig = AccelerometerConfig();
    accelerometerConfig.axisMap = "zyx";
    accelerometerConfig.axisDir = "++-";
    accelerometerConfig.motionWakeEnabled = false;
    accelerometerConfig.motionWakeThreshold = 1;
    accelerometerConfig.motionWakeDuration = 20;
    accelerometerConfig.motionWakeHighPass = 4; // default to MPU6050_HIGHPASS_0_63_HZ
    accelerometerConfig.motionWakeCycleRate = 1; // default to MPU6050_CYCLE_5_HZ
    wifiConfig = WifiConfig();
    ledConfig = LedConfig();
    gyroMouseConfig = GyroMouseConfig();
    gyroMouseConfig.enabled = false;
    gyroMouseConfig.smoothing = 0.2f;
    gyroMouseConfig.invertX = false;
    gyroMouseConfig.invertY = false;
    gyroMouseConfig.swapAxes = false;
    gyroMouseConfig.defaultSensitivity = 1;
    auto addDefaultSensitivity = [this](const char *name, float scale, float deadzone) {
        SensitivitySettings settings;
        settings.name = name;
        settings.scale = scale;
        settings.deadzone = deadzone;
        settings.mode = "gyro";
        settings.gyroScale = scale;
        settings.tiltScale = scale * 20.0f;
        settings.tiltDeadzone = deadzone;
        settings.hybridBlend = 0.0f;
        settings.invertXOverride = -1;
        settings.invertYOverride = -1;
        settings.swapAxesOverride = -1;
        this->gyroMouseConfig.sensitivities.push_back(settings);
    };

    gyroMouseConfig.sensitivities.clear();
    addDefaultSensitivity("Slow", 0.6f, 1.5f);
    addDefaultSensitivity("Medium", 1.0f, 1.2f);
    addDefaultSensitivity("Fast", 1.4f, 1.0f);
    gyroMouseConfig.absoluteRecenter = false;
    gyroMouseConfig.absoluteRangeX = 0;
    gyroMouseConfig.absoluteRangeY = 0;

    systemConfig.sleep_enabled = true;
    systemConfig.sleep_timeout_ms = 300000;
    systemConfig.sleep_timeout_mouse_ms = 0;
    systemConfig.sleep_timeout_ir_ms = 0;

    schedulerConfig = SchedulerConfig();
    schedulerConfig.enabled = false;
    schedulerConfig.preventSleepIfPending = true;
    schedulerConfig.sleepGuardSeconds = 60;
    schedulerConfig.wakeAheadSeconds = 900;
    schedulerConfig.pollIntervalMs = 250;
    schedulerConfig.events.clear();

    // Load wifi configuration if it exists
    JsonVariant wifiConfigJson = doc["wifi"];
    if (!wifiConfigJson.isNull() && wifiConfigJson.is<JsonObject>())
    {
        if (wifiConfigJson.containsKey("ap_ssid"))
            this->wifiConfig.ap_ssid = wifiConfigJson["ap_ssid"].as<String>();
        if (wifiConfigJson.containsKey("ap_password"))
            this->wifiConfig.ap_password = wifiConfigJson["ap_password"].as<String>();
        if (wifiConfigJson.containsKey("router_ssid"))
            this->wifiConfig.router_ssid = wifiConfigJson["router_ssid"].as<String>();
        if (wifiConfigJson.containsKey("router_password"))
            this->wifiConfig.router_password = wifiConfigJson["router_password"].as<String>();
    }

    JsonVariant systemConfigJson = doc["system"];
    if (!systemConfigJson.isNull() && systemConfigJson.is<JsonObject>())
    {
        if (systemConfigJson.containsKey("ap_autostart"))
            this->systemConfig.ap_autostart = systemConfigJson["ap_autostart"];
        if (systemConfigJson.containsKey("router_autostart"))
            this->systemConfig.router_autostart = systemConfigJson["router_autostart"];
        if (systemConfigJson.containsKey("enable_BLE"))
            this->systemConfig.enable_BLE = systemConfigJson["enable_BLE"];
        if (systemConfigJson.containsKey("serial_enabled"))
            this->systemConfig.serial_enabled = systemConfigJson["serial_enabled"];
        if (systemConfigJson.containsKey("sleep_enabled"))
            this->systemConfig.sleep_enabled = systemConfigJson["sleep_enabled"];

        if (systemConfigJson.containsKey("wakeup_pin"))
            this->systemConfig.wakeup_pin = systemConfigJson["wakeup_pin"];

        if (systemConfigJson.containsKey("sleep_timeout_ms"))
            this->systemConfig.sleep_timeout_ms = systemConfigJson["sleep_timeout_ms"];
        if (systemConfigJson.containsKey("sleep_timeout_mouse_ms"))
            this->systemConfig.sleep_timeout_mouse_ms = systemConfigJson["sleep_timeout_mouse_ms"];
        if (systemConfigJson.containsKey("sleep_timeout_ir_ms"))
            this->systemConfig.sleep_timeout_ir_ms = systemConfigJson["sleep_timeout_ir_ms"];

        if (systemConfigJson.containsKey("BleMacAdd"))
            this->systemConfig.BleMacAdd = systemConfigJson["BleMacAdd"];
        if (systemConfigJson.containsKey("combo_timeout"))
            this->systemConfig.combo_timeout = systemConfigJson["combo_timeout"];
        if (systemConfigJson.containsKey("BleName"))
            this->systemConfig.BleName = systemConfigJson["BleName"].as<String>();
    }

    if (this->systemConfig.sleep_timeout_mouse_ms == 0)
    {
        this->systemConfig.sleep_timeout_mouse_ms = this->systemConfig.sleep_timeout_ms;
    }
    if (this->systemConfig.sleep_timeout_ir_ms == 0)
    {
        this->systemConfig.sleep_timeout_ir_ms = this->systemConfig.sleep_timeout_ms;
    }

    // Load keypad configuration if it exists
    JsonVariant keypadConfig = doc["keypad"];
    if (!keypadConfig.isNull() && keypadConfig.is<JsonObject>())
    {
        if (keypadConfig.containsKey("rows"))
            this->keypadConfig.rows = keypadConfig["rows"];
        if (keypadConfig.containsKey("cols"))
            this->keypadConfig.cols = keypadConfig["cols"];
        if (keypadConfig.containsKey("invertDirection"))
            this->keypadConfig.invertDirection = keypadConfig["invertDirection"];

        if (keypadConfig.containsKey("rowPins"))
        {
            JsonArray rowPinsArray = keypadConfig["rowPins"];
            if (rowPinsArray)
            {
                for (JsonVariant pin : rowPinsArray)
                {
                    this->keypadConfig.rowPins.push_back(pin.as<byte>());
                }
            }
        }

        if (keypadConfig.containsKey("colPins"))
        {
            JsonArray colPinsArray = keypadConfig["colPins"];
            if (colPinsArray)
            {
                for (JsonVariant pin : colPinsArray)
                {
                    this->keypadConfig.colPins.push_back(pin.as<byte>());
                }
            }
        }

        if (keypadConfig.containsKey("keys"))
        {
            JsonArray keyArray = keypadConfig["keys"];
            if (keyArray)
            {
                for (JsonVariant row : keyArray)
                {
                    std::vector<char> keyRow;
                    JsonArray rowArray = row.as<JsonArray>();
                    if (rowArray)
                    {
                        for (JsonVariant key : rowArray)
                        {
                            keyRow.push_back(*key.as<const char *>());
                        }
                        this->keypadConfig.keys.push_back(keyRow);
                    }
                }
            }
        }
    }

    // Load encoder configuration if it exists
    JsonVariant encoderConfig = doc["encoder"];
    if (!encoderConfig.isNull() && encoderConfig.is<JsonObject>())
    {
        if (encoderConfig.containsKey("pinA"))
            this->encoderConfig.pinA = encoderConfig["pinA"];
        if (encoderConfig.containsKey("pinB"))
            this->encoderConfig.pinB = encoderConfig["pinB"];
        if (encoderConfig.containsKey("buttonPin"))
            this->encoderConfig.buttonPin = encoderConfig["buttonPin"];
        if (encoderConfig.containsKey("stepValue"))
            this->encoderConfig.stepValue = encoderConfig["stepValue"];
    }

    // Load encoder configuration if it exists
    JsonVariant ledConfigJson = doc["led"];
    if (!ledConfigJson.isNull() && ledConfigJson.is<JsonObject>())
    {
        if (ledConfigJson.containsKey("pinRed"))
            this->ledConfig.pinRed = ledConfigJson["pinRed"];
        if (ledConfigJson.containsKey("pinGreen"))
            this->ledConfig.pinGreen = ledConfigJson["pinGreen"];
        if (ledConfigJson.containsKey("pinBlue"))
            this->ledConfig.pinBlue = ledConfigJson["pinBlue"];
        if (ledConfigJson.containsKey("anodeCommon"))
            this->ledConfig.anodeCommon = ledConfigJson["anodeCommon"];
        if (ledConfigJson.containsKey("active"))
            this->ledConfig.active = ledConfigJson["active"];
        if (ledConfigJson.containsKey("brightness"))
        {
            int rawBrightness = ledConfigJson["brightness"];
            this->ledConfig.brightness = static_cast<uint8_t>(constrain(rawBrightness, 0, 255));
        }
        else
        {
            this->ledConfig.brightness = 255;
        }
    }
    else
    {
        this->ledConfig.brightness = 255;
    }

    // Load IR Sensor configuration if it exists
    JsonVariant irSensorConfig = doc["irSensor"];
    if (!irSensorConfig.isNull() && irSensorConfig.is<JsonObject>())
    {
        if (irSensorConfig.containsKey("pin"))
            this->irSensorConfig.pin = irSensorConfig["pin"];
        else
            this->irSensorConfig.pin = -1; // Default: disabled
        if (irSensorConfig.containsKey("active"))
            this->irSensorConfig.active = irSensorConfig["active"];
        else
            this->irSensorConfig.active = false;

        Logger::getInstance().log("Loaded IR Sensor config: pin=" + String(this->irSensorConfig.pin) +
                                  ", active=" + String(this->irSensorConfig.active ? "true" : "false"));
    }
    else
    {
        this->irSensorConfig.pin = -1;
        this->irSensorConfig.active = false;
        Logger::getInstance().log("IR Sensor config not found in JSON, using defaults (disabled)");
    }

    // Load IR LED configuration if it exists
    JsonVariant irLedConfig = doc["irLed"];
    if (!irLedConfig.isNull() && irLedConfig.is<JsonObject>())
    {
        if (irLedConfig.containsKey("pin"))
            this->irLedConfig.pin = irLedConfig["pin"];
        else
            this->irLedConfig.pin = -1; // Default: disabled
        if (irLedConfig.containsKey("anodeGpio"))
            this->irLedConfig.anodeGpio = irLedConfig["anodeGpio"];
        else
            this->irLedConfig.anodeGpio = false;
        if (irLedConfig.containsKey("active"))
            this->irLedConfig.active = irLedConfig["active"];
        else
            this->irLedConfig.active = false;

        Logger::getInstance().log("Loaded IR LED config: pin=" + String(this->irLedConfig.pin) +
                                  ", active=" + String(this->irLedConfig.active ? "true" : "false") +
                                  ", anodeGpio=" + String(this->irLedConfig.anodeGpio ? "true" : "false"));
    }
    else
    {
        this->irLedConfig.pin = -1;
        this->irLedConfig.anodeGpio = false;
        this->irLedConfig.active = false;
        Logger::getInstance().log("IR LED config not found in JSON, using defaults (disabled)");
    }

    // Load accelerometer configuration if it exists
    JsonVariant accelerometerConfig = doc["accelerometer"];
    if (!accelerometerConfig.isNull() && accelerometerConfig.is<JsonObject>())
    {
        if (accelerometerConfig.containsKey("sdaPin"))
            this->accelerometerConfig.sdaPin = accelerometerConfig["sdaPin"];
        if (accelerometerConfig.containsKey("sclPin"))
            this->accelerometerConfig.sclPin = accelerometerConfig["sclPin"];
        if (accelerometerConfig.containsKey("sensitivity"))
            this->accelerometerConfig.sensitivity = accelerometerConfig["sensitivity"];
        if (accelerometerConfig.containsKey("sampleRate"))
            this->accelerometerConfig.sampleRate = accelerometerConfig["sampleRate"];
        if (accelerometerConfig.containsKey("threshold"))
            this->accelerometerConfig.threshold = accelerometerConfig["threshold"];
        if (accelerometerConfig.containsKey("axisMap"))
            this->accelerometerConfig.axisMap = accelerometerConfig["axisMap"].as<const char *>();
        if (accelerometerConfig.containsKey("axisDir"))
            this->accelerometerConfig.axisDir = accelerometerConfig["axisDir"].as<const char *>();
        if (accelerometerConfig.containsKey("active"))
            this->accelerometerConfig.active = accelerometerConfig["active"];
        if (accelerometerConfig.containsKey("type"))
            this->accelerometerConfig.type = accelerometerConfig["type"].as<const char *>();
        if (accelerometerConfig.containsKey("address"))
            this->accelerometerConfig.address = accelerometerConfig["address"];
        if (accelerometerConfig.containsKey("motionWakeEnabled"))
            this->accelerometerConfig.motionWakeEnabled = accelerometerConfig["motionWakeEnabled"];
        if (accelerometerConfig.containsKey("motionWakeThreshold"))
            this->accelerometerConfig.motionWakeThreshold = accelerometerConfig["motionWakeThreshold"];
        if (accelerometerConfig.containsKey("motionWakeDuration"))
            this->accelerometerConfig.motionWakeDuration = accelerometerConfig["motionWakeDuration"];
        if (accelerometerConfig.containsKey("motionWakeHighPass"))
            this->accelerometerConfig.motionWakeHighPass = accelerometerConfig["motionWakeHighPass"];
        if (accelerometerConfig.containsKey("motionWakeCycleRate"))
            this->accelerometerConfig.motionWakeCycleRate = accelerometerConfig["motionWakeCycleRate"];
    if (accelerometerConfig.containsKey("gestureMode"))
            this->accelerometerConfig.gestureMode = accelerometerConfig["gestureMode"].as<String>();
        else
            this->accelerometerConfig.gestureMode = "auto"; // Default to auto
    }

    JsonVariant gyromouseConfigVariant = doc["gyromouse"];
    if (!gyromouseConfigVariant.isNull() && gyromouseConfigVariant.is<JsonObject>())
    {
        JsonObject gyroObj = gyromouseConfigVariant.as<JsonObject>();

        if (gyroObj.containsKey("enabled"))
            this->gyroMouseConfig.enabled = gyroObj["enabled"];
        if (gyroObj.containsKey("smoothing"))
            this->gyroMouseConfig.smoothing = gyroObj["smoothing"];
        if (gyroObj.containsKey("invertX"))
            this->gyroMouseConfig.invertX = gyroObj["invertX"];
        if (gyroObj.containsKey("invertY"))
            this->gyroMouseConfig.invertY = gyroObj["invertY"];
        if (gyroObj.containsKey("swapAxes"))
            this->gyroMouseConfig.swapAxes = gyroObj["swapAxes"];
        if (gyroObj.containsKey("defaultSensitivity"))
        {
            int idx = gyroObj["defaultSensitivity"];
            this->gyroMouseConfig.defaultSensitivity = idx < 0 ? 0 : static_cast<uint8_t>(idx);
        }

        if (gyroObj.containsKey("sensitivities") && gyroObj["sensitivities"].is<JsonArray>())
        {
            this->gyroMouseConfig.sensitivities.clear();
            for (JsonVariant entry : gyroObj["sensitivities"].as<JsonArray>())
            {
                if (!entry.is<JsonObject>())
                {
                    continue;
                }
                JsonObject sensObj = entry.as<JsonObject>();

                SensitivitySettings settings;
                settings.name = sensObj.containsKey("name") && sensObj["name"].is<const char *>()
                                    ? sensObj["name"].as<const char *>()
                                    : "mode";
                settings.scale = sensObj.containsKey("scale") ? sensObj["scale"].as<float>() : 1.0f;
                settings.deadzone = sensObj.containsKey("deadzone") ? sensObj["deadzone"].as<float>() : 1.0f;
                settings.mode = sensObj.containsKey("mode") && sensObj["mode"].is<const char *>()
                                    ? sensObj["mode"].as<const char *>()
                                    : "gyro";
                settings.gyroScale = sensObj.containsKey("gyroScale") ? sensObj["gyroScale"].as<float>() : settings.scale;
                settings.tiltScale = sensObj.containsKey("tiltScale") ? sensObj["tiltScale"].as<float>() : settings.scale * 20.0f;
                settings.tiltDeadzone = sensObj.containsKey("tiltDeadzone") ? sensObj["tiltDeadzone"].as<float>() : settings.deadzone;
                settings.hybridBlend = sensObj.containsKey("hybridBlend") ? sensObj["hybridBlend"].as<float>() : 0.35f;
                settings.hybridBlend = constrain(settings.hybridBlend, 0.0f, 1.0f);
                settings.accelerationCurve = sensObj.containsKey("accelerationCurve") ? sensObj["accelerationCurve"].as<float>() : 1.0f;
                settings.accelerationCurve = constrain(settings.accelerationCurve, 0.5f, 2.0f); // Limit to reasonable range
                if (sensObj.containsKey("invertX"))
                {
                    settings.invertXOverride = sensObj["invertX"].as<bool>() ? 1 : 0;
                }
                if (sensObj.containsKey("invertY"))
                {
                    settings.invertYOverride = sensObj["invertY"].as<bool>() ? 1 : 0;
                }
                if (sensObj.containsKey("swapAxes"))
                {
                    settings.swapAxesOverride = sensObj["swapAxes"].as<bool>() ? 1 : 0;
                }
                this->gyroMouseConfig.sensitivities.push_back(settings);
            }

            if (this->gyroMouseConfig.sensitivities.empty())
            {
                addDefaultSensitivity("Medium", 1.0f, 1.2f);
            }
        }

        if (gyroObj.containsKey("orientationAlpha"))
        {
            this->gyroMouseConfig.orientationAlpha = constrain(gyroObj["orientationAlpha"].as<float>(), 0.0f, 0.999f);
        }
        else if (this->gyroMouseConfig.orientationAlpha <= 0.0f)
        {
            this->gyroMouseConfig.orientationAlpha = 0.96f;
        }

        if (gyroObj.containsKey("tiltLimitDegrees"))
        {
            this->gyroMouseConfig.tiltLimitDegrees = gyroObj["tiltLimitDegrees"].as<float>();
        }
        else if (this->gyroMouseConfig.tiltLimitDegrees <= 0.0f)
        {
            this->gyroMouseConfig.tiltLimitDegrees = 55.0f;
        }
        this->gyroMouseConfig.tiltLimitDegrees = constrain(this->gyroMouseConfig.tiltLimitDegrees, 5.0f, 90.0f);

        if (gyroObj.containsKey("tiltDeadzoneDegrees"))
        {
            this->gyroMouseConfig.tiltDeadzoneDegrees = gyroObj["tiltDeadzoneDegrees"].as<float>();
        }
        else if (this->gyroMouseConfig.tiltDeadzoneDegrees <= 0.0f)
        {
            this->gyroMouseConfig.tiltDeadzoneDegrees = 1.5f;
        }
        this->gyroMouseConfig.tiltDeadzoneDegrees = constrain(this->gyroMouseConfig.tiltDeadzoneDegrees, 0.0f, 15.0f);

        if (gyroObj.containsKey("recenterRate"))
        {
            this->gyroMouseConfig.recenterRate = gyroObj["recenterRate"].as<float>();
        }
        else if (this->gyroMouseConfig.recenterRate < 0.0f)
        {
            this->gyroMouseConfig.recenterRate = 0.35f;
        }
        this->gyroMouseConfig.recenterRate = constrain(this->gyroMouseConfig.recenterRate, 0.0f, 1.0f);

        if (gyroObj.containsKey("recenterThresholdDegrees"))
        {
            this->gyroMouseConfig.recenterThresholdDegrees = gyroObj["recenterThresholdDegrees"].as<float>();
        }
        else if (this->gyroMouseConfig.recenterThresholdDegrees <= 0.0f)
        {
            this->gyroMouseConfig.recenterThresholdDegrees = 2.0f;
        }
        this->gyroMouseConfig.recenterThresholdDegrees = constrain(this->gyroMouseConfig.recenterThresholdDegrees, 0.1f, 20.0f);

        if (this->gyroMouseConfig.defaultSensitivity >= this->gyroMouseConfig.sensitivities.size())
        {
            this->gyroMouseConfig.defaultSensitivity = 0;
        }

        if (gyroObj.containsKey("absoluteRecenter"))
        {
            this->gyroMouseConfig.absoluteRecenter = gyroObj["absoluteRecenter"];
        }

        if (gyroObj.containsKey("absoluteRangeX"))
        {
            this->gyroMouseConfig.absoluteRangeX = gyroObj["absoluteRangeX"].as<int32_t>();
        }

        if (gyroObj.containsKey("absoluteRangeY"))
        {
            this->gyroMouseConfig.absoluteRangeY = gyroObj["absoluteRangeY"].as<int32_t>();
        }

        if (gyroObj.containsKey("clickSlowdownFactor"))
        {
            this->gyroMouseConfig.clickSlowdownFactor = gyroObj["clickSlowdownFactor"].as<float>();
        }
        else
        {
            this->gyroMouseConfig.clickSlowdownFactor = 0.3f; // Default: slow down to 30% speed
        }

        this->gyroMouseConfig.absoluteRangeX = constrain(this->gyroMouseConfig.absoluteRangeX, 0, 20000);
        this->gyroMouseConfig.absoluteRangeY = constrain(this->gyroMouseConfig.absoluteRangeY, 0, 20000);
        this->gyroMouseConfig.clickSlowdownFactor = constrain(this->gyroMouseConfig.clickSlowdownFactor, 0.0f, 1.0f);

        this->gyroMouseConfig.smoothing = constrain(this->gyroMouseConfig.smoothing, 0.0f, 1.0f);
    }

    JsonVariant schedulerJson = doc["scheduler"];
    schedulerConfig.events.clear();
    if (schedulerJson.is<JsonObject>())
    {
        JsonObject schedulerObj = schedulerJson.as<JsonObject>();
        schedulerConfig.enabled = schedulerObj["enabled"] | schedulerConfig.enabled;
        schedulerConfig.preventSleepIfPending = schedulerObj["prevent_sleep_if_pending"] | schedulerConfig.preventSleepIfPending;
        schedulerConfig.sleepGuardSeconds = schedulerObj["sleep_guard_seconds"] | schedulerConfig.sleepGuardSeconds;
        schedulerConfig.wakeAheadSeconds = schedulerObj["wake_ahead_seconds"] | schedulerConfig.wakeAheadSeconds;
        schedulerConfig.timezoneOffsetMinutes = schedulerObj["timezone_minutes"] | schedulerConfig.timezoneOffsetMinutes;
        schedulerConfig.pollIntervalMs = schedulerObj["poll_interval_ms"] | schedulerConfig.pollIntervalMs;

        JsonArray eventsArray = schedulerObj["events"].as<JsonArray>();
        if (!eventsArray.isNull())
        {
            schedulerConfig.events.reserve(eventsArray.size());
            for (JsonObject eventObj : eventsArray)
            {
                ScheduledActionConfig eventConfig;
                eventConfig.id = eventObj["id"] | "";
                if (eventConfig.id.isEmpty())
                {
                    continue;
                }

                eventConfig.description = eventObj["description"] | "";
                eventConfig.enabled = eventObj["enabled"] | true;
                eventConfig.wakeFromSleep = eventObj["wake_from_sleep"] | false;
                eventConfig.preventSleep = eventObj["prevent_sleep"] | false;
                eventConfig.runOnBoot = eventObj["run_on_boot"] | false;
                eventConfig.oneShot = eventObj["one_shot"] | false;
                eventConfig.allowOverlap = eventObj["allow_overlap"] | false;

                JsonObject triggerObj = eventObj["trigger"].as<JsonObject>();
                if (triggerObj.isNull())
                {
                    continue;
                }

                eventConfig.trigger.type = parseTriggerType(triggerObj["type"] | "interval");
                eventConfig.trigger.intervalMs = triggerObj["interval_ms"] | 0;
                eventConfig.trigger.jitterMs = triggerObj["jitter_ms"] | 0;
                eventConfig.trigger.absoluteEpoch = triggerObj["epoch"] | (time_t)0;
                eventConfig.trigger.hour = triggerObj["hour"] | 0;
                eventConfig.trigger.minute = triggerObj["minute"] | 0;
                eventConfig.trigger.second = triggerObj["second"] | 0;
                eventConfig.trigger.daysMask = parseDaysMask(triggerObj["days"]);
                eventConfig.trigger.useUtc = triggerObj["use_utc"] | false;
                eventConfig.trigger.inputSource = triggerObj["source"] | "";
                eventConfig.trigger.inputType = triggerObj["event"] | "";
                eventConfig.trigger.inputValue = triggerObj["value"] | -1;
                if (triggerObj.containsKey("state"))
                {
                    eventConfig.trigger.inputState = triggerObj["state"].as<bool>() ? 1 : 0;
                }
                else
                {
                    eventConfig.trigger.inputState = -1;
                }
                eventConfig.trigger.inputText = triggerObj["text"] | "";

                JsonObject actionObj = eventObj["action"].as<JsonObject>();
                if (actionObj.isNull())
                {
                    continue;
                }
                eventConfig.actionType = actionObj["type"] | "special_action";
                eventConfig.actionId = actionObj["id"] | "";
                if (actionObj.containsKey("params"))
                {
                    String paramsJson;
                    serializeJson(actionObj["params"], paramsJson);
                    eventConfig.actionParams = paramsJson;
                }
                else
                {
                    eventConfig.actionParams = "";
                }

                schedulerConfig.events.push_back(eventConfig);
            }
        }
    }

    configFile.close();
    return true;
}

const KeypadConfig &ConfigurationManager::getKeypadConfig() const
{
    return keypadConfig;
}

const EncoderConfig &ConfigurationManager::getEncoderConfig() const
{
    return encoderConfig;
}

const AccelerometerConfig &ConfigurationManager::getAccelerometerConfig() const
{
    return accelerometerConfig;
}

const GyroMouseConfig &ConfigurationManager::getGyroMouseConfig() const
{
    return gyroMouseConfig;
}

const WifiConfig &ConfigurationManager::getWifiConfig() const
{
    return wifiConfig;
}
const LedConfig &ConfigurationManager::getLedConfig() const
{
    return ledConfig;
}

bool ConfigurationManager::setLedBrightness(uint8_t brightness)
{
    ledConfig.brightness = brightness;

    if (!FileSystemManager::ensureMounted())
    {
        Logger::getInstance().log("ConfigurationManager: failed to mount LittleFS for brightness update");
        return false;
    }

    File configFile = LittleFS.open("/config.json", "r");
    if (!configFile)
    {
        Logger::getInstance().log("ConfigurationManager: failed to open config.json for reading (brightness update)");
        return false;
    }

    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close();
    if (error)
    {
        Logger::getInstance().log("ConfigurationManager: failed to parse config.json (brightness update): " + String(error.c_str()));
        return false;
    }

    JsonObject ledObj = doc["led"].isNull() ? doc.createNestedObject("led") : doc["led"].as<JsonObject>();
    ledObj["brightness"] = brightness;

    File outFile = LittleFS.open("/config.json", "w");
    if (!outFile)
    {
        Logger::getInstance().log("ConfigurationManager: failed to open config.json for writing (brightness update)");
        return false;
    }

    if (serializeJsonPretty(doc, outFile) == 0)
    {
        Logger::getInstance().log("ConfigurationManager: failed to write brightness to config.json");
        outFile.close();
        return false;
    }

    outFile.close();
    return true;
}

const IRSensorConfig &ConfigurationManager::getIrSensorConfig() const
{
    return irSensorConfig;
}

const IRLedConfig &ConfigurationManager::getIrLedConfig() const
{
    return irLedConfig;
}

const SchedulerConfig &ConfigurationManager::getSchedulerConfig() const
{
    return schedulerConfig;
}

const SystemConfig &ConfigurationManager::getSystemConfig() const
{
    return systemConfig;
}
