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
#include "gestureRead.h"
#include <driver/rtc_io.h>
PowerManager::PowerManager() : lastActivityTime(0),
                               inactivityTimeout(300000), // Default 5 minutes
                               sleepEnabled(true),
                               isBleMode(false),
                               wakeupPin(GPIO_NUM_0), // Default pin
                               fallbackWakePin(GPIO_NUM_NC),
                               fallbackWakePinValid(false)
{
}

void PowerManager::begin(const SystemConfig &sysConfig, const KeypadConfig &keypadConfig, const EncoderConfig &encoderConfig)
{
    // Initialize with config values
    sleepEnabled = sysConfig.sleep_enabled;
    inactivityTimeout = sysConfig.sleep_timeout_ms;
    isBleMode = sysConfig.enable_BLE;

    wakeupPin = sysConfig.wakeup_pin;
    fallbackWakePin = static_cast<gpio_num_t>(encoderConfig.buttonPin);
    fallbackWakePinValid = rtc_gpio_is_valid_gpio(fallbackWakePin);

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

    bool motionWakeActive = gestureSensor.isMotionWakeEnabled();
    gpio_num_t effectiveWakePin = wakeupPin;
    bool usingFallback = false;

    if (!motionWakeActive && fallbackWakePinValid)
    {
        effectiveWakePin = fallbackWakePin;
        usingFallback = true;
    }

    // Calibrate accelerometer before entering sleep to ensure stable state
    Logger::getInstance().log("Calibrating accelerometer before sleep...");
    if (gestureSensor.calibrate(10))
    {
        Logger::getInstance().log("Accelerometer calibrated successfully before sleep");
    }
    else
    {
        Logger::getInstance().log("⚠️ Accelerometer calibration failed before sleep");
    }

    if (!rtc_gpio_is_valid_gpio(effectiveWakePin))
    {
        Logger::getInstance().log("⚠️ No valid wake pin configured for deep sleep ext0 (motionWake=" + String(motionWakeActive ? "true" : "false") + ")");
    }
    else
    {
        pinMode(static_cast<uint8_t>(effectiveWakePin), INPUT_PULLUP);
        int wakeLevel = digitalRead(static_cast<uint8_t>(effectiveWakePin));
        String pinLog = "Wake pin (" + String(static_cast<int>(effectiveWakePin)) + (usingFallback ? ", fallback" : "") + ") level before sleep: ";
        Logger::getInstance().log(pinLog + String(wakeLevel == LOW ? "LOW" : "HIGH"));

        if (motionWakeActive && !usingFallback)
        {
            Logger::getInstance().log("Motion wake enabled on GPIO " + String(static_cast<int>(wakeupPin)));
            if (!gestureSensor.standby())
            {
                Logger::getInstance().log("Failed to rearm accelerometer motion wake before sleep");
            }
            else
            {
                // Wait for sensor to stabilize after entering standby mode
                vTaskDelay(pdMS_TO_TICKS(50));

                wakeLevel = digitalRead(static_cast<uint8_t>(effectiveWakePin));
                Logger::getInstance().log("Wake pin level after standby: " + String(wakeLevel == LOW ? "LOW" : "HIGH"));

                // Retry loop to clear spurious interrupts
                const uint8_t maxClearAttempts = 3;
                for (uint8_t attempt = 0; attempt < maxClearAttempts && wakeLevel == LOW; attempt++)
                {
                    Logger::getInstance().log("Motion interrupt active (attempt " + String(attempt + 1) + "/" + String(maxClearAttempts) + "): clearing");

                    if (!gestureSensor.clearMotionWakeInterrupt())
                    {
                        Logger::getInstance().log("Failed to clear motion interrupt");
                        break;
                    }

                    // Longer delay to allow sensor to stabilize
                    vTaskDelay(pdMS_TO_TICKS(100));
                    wakeLevel = digitalRead(static_cast<uint8_t>(effectiveWakePin));
                    Logger::getInstance().log("Wake pin level after clear: " + String(wakeLevel == LOW ? "LOW" : "HIGH"));
                }

                if (wakeLevel == LOW)
                {
                    Logger::getInstance().log("⚠️ Wake pin still LOW after " + String(maxClearAttempts) + " attempts; motion wake may fire immediately");
                }
            }
        }
        else
        {
            if (usingFallback)
            {
                Logger::getInstance().log("Motion wake disabled; using fallback wake pin " + String(static_cast<int>(effectiveWakePin)));
            }
            else
            {
                Logger::getInstance().log("Motion wake not active; relying on standard wake sources");
            }

            if (!gestureSensor.standby())
            {
                Logger::getInstance().log("Gesture sensor standby failed before sleep");
            }
        }

        esp_err_t wakeErr = esp_sleep_enable_ext0_wakeup(effectiveWakePin, LOW);
        if (wakeErr != ESP_OK)
        {
            Logger::getInstance().log("Failed to enable EXT0 wakeup on pin " + String(static_cast<int>(effectiveWakePin)) + ": err " + String(wakeErr));
        }
    }

    // Configure wakeup sources first (before animation)
    esp_sleep_enable_timer_wakeup(28800000000ULL); // 8h backup
    // Comprehensive sleep parameters table
    Logger::getInstance().log("╔═════════════════════════════════════════════════╗");
    Logger::getInstance().log("║                  🔋 SLEEP PARAMS                 ║");
    Logger::getInstance().log("╠═════════════════════════════════════════════════╣");
    Logger::getInstance().log("║ Timeout (s):  " + String(inactivityTimeout / 1000) + String(" ").substring(0, 18 - String(inactivityTimeout / 1000).length()) + "║");
    Logger::getInstance().log("║ Wakeup Pin:   " + String(wakeupPin) + String(" ").substring(0, 18 - String(wakeupPin).length()) + "║");
    Logger::getInstance().log("║ Backup Time:  8h" + String(" ").substring(0, 20) + "║");
    Logger::getInstance().log("║ Free Memory:  " + String(ESP.getFreeHeap() / 1024) + " KB" + String(" ").substring(0, 16 - String(ESP.getFreeHeap() / 1024).length()) + "║");
    Logger::getInstance().log("║ Uptime:       " + String(millis() / 60000) + " m" + String(" ").substring(0, 18 - String(millis() / 60000).length()) + "║");
    Logger::getInstance().log("║ Mode:         BLE" + String(" ").substring(0, 20) + "║");
    Logger::getInstance().log("║ Next Wake:    Button/8h" + String(" ").substring(0, 15) + "║");
    Logger::getInstance().log("╚═════════════════════════════════════════════════╝");
    Logger::getInstance().processBuffer();
    vTaskDelay(pdMS_TO_TICKS(100)); // Dai tempo al tempo

    Logger::getInstance().processBuffer();
    vTaskDelay(pdMS_TO_TICKS(500)); // Dai tempo al tempo


    // Enter deep sleep
    esp_deep_sleep_start();

    // Code continues from setup() after wakeup
}
