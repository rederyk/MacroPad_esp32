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

class ConfigurationManager {
private:
    KeypadConfig keypadConfig;
    EncoderConfig encoderConfig;
    AccelerometerConfig accelerometerConfig;
    JsonVariant combinations; // Store combinations with more flexible type
    SystemConfig systemConfig;


public:
    ConfigurationManager();
    bool loadConfig();
    const KeypadConfig& getKeypadConfig() const;
    const EncoderConfig& getEncoderConfig() const;
    const AccelerometerConfig& getAccelerometerConfig() const;
    JsonVariant getCombinations() const;
    const WifiConfig& getWifiConfig() const;
    const SystemConfig& getSystemConfig() const;
private:
    WifiConfig wifiConfig;
};

#endif
