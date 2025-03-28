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
#include "gestureRead.h"
#include "Logger.h"

ConfigurationManager::ConfigurationManager() : systemConfig() {}

bool ConfigurationManager::loadConfig()
{
    if (!LittleFS.begin(true))
    {
        Logger::getInstance().log("Failed to mount LittleFS");
        return false;
    }

    File configFile = LittleFS.open("/config.json");
    if (!configFile)
    {
        Logger::getInstance().log("Failed to open config file");
        LittleFS.end();
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

    // Increase buffer size and add error handling
    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, configFile, DeserializationOption::Filter(filter));
    if (error)
    {
        Logger::getInstance().log("Failed to parse config file: " + String(error.c_str()));
        configFile.close();
        LittleFS.end();
        return false;
    }

    // Clear any existing data
    keypadConfig = KeypadConfig();
    encoderConfig = EncoderConfig();
    accelerometerConfig = AccelerometerConfig();
    wifiConfig = WifiConfig();
    ledConfig = LedConfig();

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

        if (systemConfigJson.containsKey("BleMacAdd"))
            this->systemConfig.BleMacAdd = systemConfigJson["BleMacAdd"];
        if (systemConfigJson.containsKey("combo_timeout"))
            this->systemConfig.combo_timeout = systemConfigJson["combo_timeout"];
        if (systemConfigJson.containsKey("BleName"))
            this->systemConfig.BleName = systemConfigJson["BleName"].as<String>();
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
    JsonVariant ledConfig = doc["led"];
    if (!ledConfig.isNull() && ledConfig.is<JsonObject>())
    {
        if (ledConfig.containsKey("pinRed"))
            this->ledConfig.pinRed = ledConfig["pinRed"];
        if (ledConfig.containsKey("pinGreen"))
            this->ledConfig.pinGreen = ledConfig["pinGreen"];
        this->ledConfig.pinRed = ledConfig["pinRed"];
        if (ledConfig.containsKey("pinBlue"))
            this->ledConfig.pinBlue = ledConfig["pinBlue"];
        if (ledConfig.containsKey("anodeCommon"))
            this->ledConfig.anodeCommon = ledConfig["anodeCommon"];
        if (ledConfig.containsKey("active"))
            this->ledConfig.active = ledConfig["active"];
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
        if (accelerometerConfig.containsKey("active"))
            this->accelerometerConfig.active = accelerometerConfig["active"];
    }

    configFile.close();
    LittleFS.end();
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

const WifiConfig &ConfigurationManager::getWifiConfig() const
{
    return wifiConfig;
}
const LedConfig &ConfigurationManager::getLedConfig() const
{
    return ledConfig;
}

const SystemConfig &ConfigurationManager::getSystemConfig() const
{
    return systemConfig;
}
