#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include "Logger.h"
#include "configManager.h"
#include <vector>

class PowerManager
{
private:
    unsigned long lastActivityTime;
    unsigned long inactivityTimeout; // in milliseconds
    bool sleepEnabled;
    bool isBleMode;
    gpio_num_t wakeupPin;
    gpio_num_t wakeupPin2;

public:
    PowerManager();

    void begin(const SystemConfig &sysConfig, const KeypadConfig &keypadConfig);
    void resetActivityTimer();
    void registerActivity();
    bool checkInactivity();
    void enterDeepSleep();
};

#endif // POWER_MANAGER_H