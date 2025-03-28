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

    wakeupPin = sysConfig.wakeup_pin;

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

void PowerManager::enterDeepSleep(bool force)
{
    if (!isBleMode&&!force)
    {
        Logger::getInstance().log("⚠️ Sleep mode only available in BLE mode ⚠️");
        return;
    }

    // ASCII visualization of sleep parameters
    Logger::getInstance().log("┌─────────────────────────────────────┐");
    Logger::getInstance().log("│ 🔋 SLEEP PARAMS                     │");
    Logger::getInstance().log("├─────────────┬─────────┬────────────┤");
    Logger::getInstance().log("│ ⏱️  Timeout  │ 🔔 Wake │ ⏰ Backup   │");
    delay(20);
    Logger::getInstance().log("│ " + String(inactivityTimeout / 1000) + "s" + String("          ").substring(0, 10 - String(inactivityTimeout / 1000).length()) + "│ PIN" + String(wakeupPin) + String("     ").substring(0, 7 - String(wakeupPin).length()) + "│ 8h        │");
    delay(20);

    Logger::getInstance().log("└─────────────┴─────────┴────────────┘");
    delay(20);

    Logger::getInstance().processBuffer();

    // Configure wakeup sources first (before animation)
    esp_sleep_enable_ext0_wakeup(wakeupPin, LOW);
    esp_sleep_enable_timer_wakeup(28800000000ULL); // 8h backup

    // Enhanced final sleep box with system stats
    Logger::getInstance().log("╔══════════════════════════════════╗");
    Logger::getInstance().log("║ 💤 DEEP SLEEP MODE ACTIVATED 💤  ║");
    Logger::getInstance().log("╠══════════════════════════════════╣");
    delay(20);

    Logger::getInstance().log("║ Mem: " + String(ESP.getFreeHeap() / 1024) + "KB | Uptime: " + String(millis() / 60000) + "m ║");
    delay(20);

    Logger::getInstance().log("║ Mode: BLE | Next wake: Button/8h ║");
    Logger::getInstance().log("╚══════════════════════════════════╝");
    delay(20);

    Logger::getInstance().processBuffer();
    delay(500);


    // Enter deep sleep
    esp_deep_sleep_start();

    // Code continues from setup() after wakeup
}