#include "powerManager.h"
#include <vector>
PowerManager::PowerManager() : lastActivityTime(0),
                               inactivityTimeout(300000), // Default 5 minutes
                               sleepEnabled(true),
                               isBleMode(false),
                               wakeupPin(GPIO_NUM_0) // Default pin
{
}

void PowerManager::begin(const SystemConfig &sysConfig, const KeypadConfig &keypadConfig)
{
    // Initialize with config values
    sleepEnabled = sysConfig.sleep_enabled;
    inactivityTimeout = sysConfig.sleep_timeout_ms;
    isBleMode = sysConfig.enable_BLE;

    // Set wakeup pin from keypad config (assuming first key as wakeup pin)
    if (keypadConfig.rowPins.size() > 0 && keypadConfig.colPins.size() > 0)
    {
        wakeupPin = static_cast<gpio_num_t>(keypadConfig.colPins[0]);
    }
    wakeupPin2 = sysConfig.wakeup_pin2;

    // Reset activity timer
    resetActivityTimer();

    // Check if we just woke from deep sleep
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason != ESP_SLEEP_WAKEUP_UNDEFINED)
    {
        Logger::getInstance().log("Woke up from sleep, reason: " + String(wakeup_reason));
    }
}

void PowerManager::resetActivityTimer()
{
    lastActivityTime = millis();
}

void PowerManager::registerActivity()
{
    resetActivityTimer();
}

bool PowerManager::checkInactivity()
{
    if (!sleepEnabled || !isBleMode)
    {
        return false; // Sleep disabled or not in BLE mode
    }

    return (millis() - lastActivityTime > inactivityTimeout);
}



void PowerManager::enterDeepSleep()
{
    if (!isBleMode)
    {
        Logger::getInstance().log("Sleep mode only available in BLE mode");
        return;
    }

    Logger::getInstance().log(String(inactivityTimeout/1000)+String(" Second while last input Enter deep sleep mode"));
    Logger::getInstance().log(String("pin di wake up impostato su <BUTTON> pin ") + String(wakeupPin2));

    // Configure wakeup sources
    esp_sleep_enable_ext0_wakeup(wakeupPin2, LOW); // LOW level for button press

    // Also enable timer wakeup as a backup (8 hours)
    esp_sleep_enable_timer_wakeup(28800000000ULL); // 8 hours in microseconds

    // Flush logger before sleep
    Logger::getInstance().processBuffer();

    // Perform any necessary cleanup
    // Note: We don't need to explicitly disconnect BLE here as ESP_DEEP_SLEEP_START will
    // reset most peripherals. If needed, specific resources can be released before sleep.

    // Enter deep sleep
    esp_deep_sleep_start();

    // Code will continue from setup() after wakeup
}