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



#ifndef SPECIAL_ACTION_H
#define SPECIAL_ACTION_H

#include <Arduino.h>

class SpecialAction
{
public:
    enum class LedMode {
        NONE,
        REACTIVE,
        IR_SCAN,
        IR_SEND,
        FLASHLIGHT
    };

    LedMode getCurrentLedMode() const { return currentLedMode; }

    /// System commands
    void resetDevice();
    void actionDelay(int totalDelayMs);

    /// Sensor management
    void calibrateSensor();
    String getGestureID();

    void printMemoryInfo();
    void executeGesture(bool pressed);
    void hopBleDevice();
    void toggleBleWifi();
    void enterSleep();
    void toggleAP(bool toogle);

    /// IR remote control management (toggle pattern)
    void toggleScanIR(int deviceId, const String &exitCombo = "");             // Toggle IR scan mode (auto-switches between devices)
    void toggleSendIR(int deviceId, const String &exitCombo = "");             // Toggle IR send mode (auto-switches between devices)
    void sendIRCommand(const String &deviceName, const String &commandName); // Send specific IR command immediately (supports any device/command names)
    void checkIRSignal();                        // Check current IR signal (for debugging)

    /// LED RGB control
    void setLedColor(int red, int green, int blue, bool save = false);         // Set LED color (NO brightness for manual control)
    void setSystemLedColor(int red, int green, int blue, bool save = false);   // Set LED color with brightness (for system notifications)
    void adjustLedColor(int redDelta, int greenDelta, int blueDelta);          // Adjust RGB values relatively (with clamp)
    void turnOffLed();                                                         // Turn off LED (set to 0,0,0)
    void saveLedColor();                                                       // Save current LED color (scaled values)
    void restoreLedColor();                                                    // Restore saved LED color (scaled values)
    void saveSystemLedColor();                                                 // Save current system LED color (original + brightness)
    void restoreSystemLedColor();                                              // Restore saved system LED color (reapply brightness)
    void showLedInfo();                                                        // Show current LED color info in log
    int ledAdjustmentStep = 5;                                                 // Default step for PLUS/MINUS (configurable)

    /// LED Brightness control (persistent)
    void setBrightness(int brightness);                                 // Set brightness 0-255 (saves to file)
    void adjustBrightness(int delta);                                   // Adjust brightness relatively
    int getBrightness();                                                // Get current brightness
    void loadBrightness();                                              // Load brightness from file
    void showBrightnessInfo();                                          // Show current brightness in log
    void toggleFlashlight();                                            // Toggle flashlight mode
    int brightnessAdjustmentStep = 10;                                  // Default step for brightness PLUS/MINUS (configurable)

    void setReactiveLightingActive(bool active);                        // Mark reactive lighting as owner of the LED
    bool isReactiveLightingActive() const { return reactiveLightingActive; }
    bool isIrModeActive() const;

private:
    void applyDeferredSystemLedColor();

    LedMode currentLedMode = LedMode::NONE;
    bool flashlightActive = false;
    int flashlightSavedColor[3] = {0, 0, 0};
    int getKeypadInput(unsigned long timeout);
    int getInput(unsigned long timeout, bool allowGesture = false); // Keypad + optional gesture input
    String getInputWithEncoder(unsigned long timeout, bool allowGesture = false, bool allowEncoder = true, const String &exitCombo = ""); // Keypad + gesture + encoder (CW/CCW/BUTTON)

    // LED Brightness storage
    int currentBrightness = 255;                                    // Current brightness (0-255), default 255
    void saveBrightnessToFile();                                    // Save brightness to persistent storage

    // Valori RGB originali (prima dello scaling del brightness)
    int originalRed = 255;
    int originalGreen = 255;
    int originalBlue = 255;

    // Saved system LED values (for temporary operations like sampling, reactive lighting)
    int savedSystemRed = 255;
    int savedSystemGreen = 255;
    int savedSystemBlue = 255;
    bool systemColorSaved = false;
    bool reactiveLightingActive = false;
    bool systemColorDeferred = false;
    bool deferredSystemSave = false;
    bool deferredSystemLogged = false;
    int deferredSystemRed = 0;
    int deferredSystemGreen = 0;
    int deferredSystemBlue = 0;
};

#endif
