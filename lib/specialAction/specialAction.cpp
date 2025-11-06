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

#include "specialAction.h"
#include <esp_system.h>
#include <Arduino.h>
#include <gestureRead.h>
#include <gestureAnalyze.h>
#include <LittleFS.h>
#include "keypad.h"
#include "Logger.h"
#include "powerManager.h"
#include "IRSensor.h"
#include "IRSender.h"
#include "IRStorage.h"
#include <ArduinoJson.h>
#include "rotaryEncoder.h"
#include "Led.h"
#include "configManager.h"
#include "InputHub.h"
#include "FileSystemManager.h"

extern InputHub inputHub;

extern GestureRead gestureSensor;
extern GestureAnalyze gestureAnalyzer;
extern PowerManager powerManager;
extern ConfigurationManager configManager;

void SpecialAction::resetDevice()
{
    ESP.restart();
}
void SpecialAction::enterSleep()
{

    powerManager.enterDeepSleep(true);
}

void SpecialAction::actionDelay(int totalDelayMs)
{
    // Calcola il numero di passi basato su 100ms per puntino
    const int dotIntervalMs = 100;                                  // Un puntino ogni 100ms
    int steps = (totalDelayMs + dotIntervalMs - 1) / dotIntervalMs; // Arrotonda per eccesso
    int intervalMs = dotIntervalMs;

    // Simboli per migliorare la visualizzazione
    const char *symbols[] = {"⏱️", "⌛", "⏳"};
    int symbolIndex = 0;

    // Esegue il countdown
    for (int i = steps; i >= 0; i--)
    {
        // Cambia simbolo ogni 3 iterazioni per creare animazione
        symbolIndex = (steps - i) % 3;

        // Calcola ms rimanenti
        int remainingMs = i * intervalMs;
        if (remainingMs > totalDelayMs)
            remainingMs = totalDelayMs; // Previene valori superiori al totale

        // Formatta i ms: se > 1000, mostra secondi, altrimenti ms
        std::string timeDisplay;
        if (remainingMs >= 1000)
        {
            float seconds = remainingMs / 1000.0f;
            char buffer[10];
            sprintf(buffer, "%.1fs", seconds);
            timeDisplay = buffer;
        }
        else
        {
            timeDisplay = std::to_string(remainingMs) + "ms";
        }

        // Crea barra di progresso con puntini (un puntino ogni 100ms circa)
        std::string progressBar = "[";
        for (int j = 0; j < steps; j++)
        {
            progressBar += (j < i) ? "." : " ";
        }
        progressBar += "]";

        // Log pulito e informativo
        Logger::getInstance().log(symbols[symbolIndex] + String(" ") +
                                  String(progressBar.c_str()) + " " +
                                  String(timeDisplay.c_str()));
        Logger::getInstance().processBuffer();

        if (i > 0)
        {

            vTaskDelay(pdMS_TO_TICKS(intervalMs)); // Dai tempo al tempo
        }
    }
}

int SpecialAction::getKeypadInput(unsigned long timeout)
{
    unsigned long startTime = millis();
    int key = -1;
    Keypad *keypadDevice = inputHub.getKeypad();

    if (!keypadDevice)
    {
        Logger::getInstance().log("Keypad not available");
        return -1;
    }

    inputHub.clearQueue();

    while (key < 0 || key > 8)
    {
        if (keypadDevice->processInput())
        {
            InputEvent event = keypadDevice->getEvent();
            if (event.type == InputEvent::EventType::KEY_PRESS && event.state)
            {
                key = event.value1; // Get key code (0-8)
                break;
            }
        }

        // Timeout
        if (millis() - startTime > timeout)
        {
            Logger::getInstance().log("Timeout");
            inputHub.clearQueue();
            return -1;
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Dai tempo al logger
    }
    inputHub.clearQueue();
    return key;
}

int SpecialAction::getInput(unsigned long timeout, bool allowGesture)
{
    unsigned long startTime = millis();
    int value = -1;
    Keypad *keypadDevice = inputHub.getKeypad();

    if (!keypadDevice)
    {
        Logger::getInstance().log("Keypad not available");
        return -1;
    }

    inputHub.clearQueue();

    while (value < 0 || value > 8)
    {
        // Check keypad input
        if (keypadDevice->processInput())
        {
            InputEvent event = keypadDevice->getEvent();
            if (event.type == InputEvent::EventType::KEY_PRESS && event.state)
            {
                value = event.value1; // Get key code (0-8)
                Logger::getInstance().log("Input from keypad: " + String(value + 1));
                break;
            }
        }

        // Check gesture if allowed
        if (allowGesture)
        {
            extern GestureAnalyze gestureAnalyzer;
            String gestureResult = getGestureID();
            if (gestureResult.startsWith("G_ID:"))
            {
                String idStr = gestureResult.substring(5); // After "G_ID:"
                int gestureId = idStr.toInt();
                if (gestureId >= 0 && gestureId <= 7)
                {
                    value = gestureId;
                    Logger::getInstance().log("Input from gesture: " + String(value + 1));
                    break;
                }
            }
        }

        // Timeout
        if (millis() - startTime > timeout)
        {
            Logger::getInstance().log("Input timeout");
            inputHub.clearQueue();
            return -1;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    inputHub.clearQueue();
    return value;
}

String SpecialAction::getInputWithEncoder(unsigned long timeout, bool allowGesture, bool allowEncoder, const String &exitCombo)
{
    unsigned long startTime = millis();
    String result = "";
    Keypad *keypadDevice = inputHub.getKeypad();
    RotaryEncoder *rotaryDevice = inputHub.getRotaryEncoder();

    if (!keypadDevice)
    {
        Logger::getInstance().log("Keypad not available");
        return "";
    }

    if (allowEncoder && !rotaryDevice)
    {
        Logger::getInstance().log("Rotary encoder not available");
        allowEncoder = false;
    }

    inputHub.clearQueue();

    // Track pressed keys for exit combo detection
    uint16_t pressedKeys = 0; // Bitmask for keys 0-15
    uint16_t lastPressedKeys = 0; // Remember what was pressed before release
    bool checkingExitCombo = (exitCombo.length() > 0);

    while (result == "")
    {
        // Check keypad input
        if (keypadDevice->processInput())
        {
            InputEvent event = keypadDevice->getEvent();

            // Track key presses/releases for exit combo
            if (checkingExitCombo && event.type == InputEvent::EventType::KEY_PRESS)
            {
                if (event.state) // Key pressed
                {
                    pressedKeys |= (1 << event.value1);
                    lastPressedKeys = pressedKeys; // Save current combo
                }
                else // Key released
                {
                    pressedKeys &= ~(1 << event.value1);

                    // When all keys are released, check what was pressed
                    if (pressedKeys == 0 && lastPressedKeys != 0)
                    {
                        // Build the combo string from lastPressedKeys
                        String pressedCombo = "";
                        for (int i = 0; i < 9; i++)
                        {
                            if (lastPressedKeys & (1 << i))
                            {
                                if (pressedCombo.length() > 0) pressedCombo += "+";
                                pressedCombo += String(i + 1);
                            }
                        }

                        // FIRST: Check if it was the exit combo (highest priority)
                        if (pressedCombo == exitCombo || pressedCombo + ",BUTTON" == exitCombo)
                        {
                            Logger::getInstance().log("Exit combo detected: " + exitCombo);
                            return "EXIT_COMBO";
                        }

                        // Check if it was a single key
                        int keyCount = 0;
                        int singleKey = -1;
                        for (int i = 0; i < 9; i++)
                        {
                            if (lastPressedKeys & (1 << i))
                            {
                                keyCount++;
                                singleKey = i;
                            }
                        }

                        if (keyCount == 1)
                        {
                            // Single key: return as "cmd#" format
                            result = "cmd" + String(singleKey + 1);
                            Logger::getInstance().log("Input from keypad: " + result);
                            lastPressedKeys = 0;
                            break;
                        }
                        else
                        {
                            // Multi-key combo (but not exit combo): use combo as command name
                            result = pressedCombo;
                            Logger::getInstance().log("Input combo: " + result);
                            lastPressedKeys = 0;
                            break;
                        }
                    }
                }
            }
            // Handle keypad when not checking for exit combo
            else if (!checkingExitCombo && event.type == InputEvent::EventType::KEY_PRESS && event.state)
            {
                int key = event.value1;
                if (key >= 0 && key <= 8)
                {
                    result = "cmd" + String(key + 1);
                    Logger::getInstance().log("Input from keypad: " + result);
                    break;
                }
            }
        }

        // Check encoder input
        if (allowEncoder && rotaryDevice->processInput())
        {
            InputEvent event = rotaryDevice->getEvent();

            // Handle encoder rotation
            if (event.type == InputEvent::EventType::ROTATION && event.state)
            {
                result = (event.value1 > 0) ? "CW" : "CCW";
                Logger::getInstance().log("Input from encoder: " + result);
                break;
            }

            // Handle encoder button press
            if (event.type == InputEvent::EventType::BUTTON && event.state)
            {
                result = "BUTTON";
                Logger::getInstance().log("Input from encoder button: " + result);
                break;
            }
        }

        // Check gesture if allowed
        if (allowGesture)
        {
            String gestureResult = getGestureID();
            if (gestureResult.startsWith("G_ID:"))
            {
                String idStr = gestureResult.substring(5); // After "G_ID:"
                int gestureId = idStr.toInt();
                if (gestureId >= 0 && gestureId <= 7)
                {
                    result = "G_" + String(gestureId + 1); // Prefix with G_ to distinguish from keypad
                    Logger::getInstance().log("Input from gesture: " + result);
                    break;
                }
            }
        }

        // Timeout
        if (millis() - startTime > timeout)
        {
            Logger::getInstance().log("Input timeout");
            inputHub.clearQueue();
            return "";
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    inputHub.clearQueue();
    return result;
}

void SpecialAction::calibrateSensor()
{
    if (!configManager.getAccelerometerConfig().active)
    {
        Logger::getInstance().log("Accelerometer disabled");
        return;
    }
    if (gestureSensor.calibrate())
    {
        Logger::getInstance().log("Calibration successful!");
    }
    else
    {
        Logger::getInstance().log("Calibration failed!");
    }
}

String SpecialAction::getGestureID()
{
    if (!configManager.getAccelerometerConfig().active || !inputHub.hasGestureSensor())
    {
        Logger::getInstance().log("Accelerometer disabled");
        return "";
    }

    if (inputHub.isGestureCapturing())
    {
        return ""; // No gesture recognized yet
    }
    int gestureId = inputHub.getLastGestureId();
    String gestureName = inputHub.getLastGestureName();

    if (gestureName.length() > 0)
    {
        return gestureName;
    }

    if (gestureId < 0)
    {
        return "";
    }

    String result = "G_ID:";
    result += String(gestureId);
    return result;
}

void SpecialAction::printMemoryInfo()
{
    Logger::getInstance().log(String("Free heap, with esp_get_free_heap_size: ") + String(esp_get_free_heap_size()).c_str());

    Logger::getInstance().log(String("Flash chip size: ") + String(ESP.getFlashChipSize()).c_str());

    // Log free RAM memory
    Logger::getInstance().log(String("Free Ram memory: ") + String(ESP.getFreeHeap()).c_str() + String(" bytes"));

    // Log free flash memory
    Logger::getInstance().log(String("Free flash memory: ") + String(ESP.getFlashChipSize() - ESP.getSketchSize()).c_str() + String(" bytes"));

    // Log LittleFS information
    if (FileSystemManager::ensureMounted(false))
    {
        Logger::getInstance().log((String("LittleFS total space: ") + String(LittleFS.totalBytes())).c_str());
        Logger::getInstance().log(String("LittleFS used space: ") + String(LittleFS.usedBytes()).c_str());
    }
    else
    {
        Logger::getInstance().log("LittleFS Mount Failed");
    }
}

void SpecialAction::hopBleDevice()
{
    Logger::getInstance().log("Press key 1-9 to select BLE device");

    int key = getKeypadInput(5000);

    if (key < 0 || key > 8)
    {
        Logger::getInstance().log("Invalid key");
        return;
    }

    if (FileSystemManager::ensureMounted())
    {
        File configFile = LittleFS.open("/config.json", "r");
        if (!configFile)
        {
            Logger::getInstance().log("Failed to open config file");
            return;
        }

        size_t size = configFile.size();
        if (size > 4096)
        {
            Logger::getInstance().log("Config file size is too large");
            configFile.close();
            return;
        }

        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        configFile.close();

        DynamicJsonDocument doc(4096);
        DeserializationError error = deserializeJson(doc, buf.get());
        if (error)
        {
            Logger::getInstance().log(String("Failed to parse config file") + String(error.c_str()));
            return;
        }

        // **Aggiorna il valore di BleMacAdd con il numero premuto**
        doc["system"]["BleMacAdd"] = key;

        // **Salva le modifiche nel file**
        File outFile = LittleFS.open("/config.json", "w");
        if (!outFile)
        {
            Logger::getInstance().log("Failed to open config file for writing");
            return;
        }

        serializeJson(doc, outFile);
        outFile.close();

        Logger::getInstance().log(String("BleMacAdd updated to: ") + String(key));
    }

    ESP.restart();
}

void SpecialAction::toggleBleWifi()
{
    if (!FileSystemManager::ensureMounted())
    {
        Logger::getInstance().log("Failed to initialize LittleFS");
        return;
    }

    File configFile = LittleFS.open("/config.json", "r");
    if (!configFile)
    {

        /// non si apre il file.....??????
        Logger::getInstance().log("Failed to open config file");
        return;
    }

    size_t size = configFile.size();
    if (size > 4096)
    {
        Logger::getInstance().log("Config file size is too large");
        configFile.close();
        return;
    }

    std::unique_ptr<char[]> buf(new char[size]);
    configFile.readBytes(buf.get(), size);
    configFile.close();

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, buf.get());
    if (error)
    {
        Logger::getInstance().log(String("Failed to parse config file") + String(error.c_str()));
        return;
    }

    // **Legge il valore attuale di enable_BLE**
    bool enable_BLE = doc["system"]["enable_BLE"];

    // **Inverti enable_BLE e aggiorna gli altri valori**
    enable_BLE = !enable_BLE;
    doc["system"]["enable_BLE"] = enable_BLE;
    doc["system"]["router_autostart"] = enable_BLE ? false : true;

    // **Salva le modifiche nel file**
    File outFile = LittleFS.open("/config.json", "w");
    if (!outFile)
    {
        Logger::getInstance().log("Failed to open config file for writing");
        return;
    }

    serializeJson(doc, outFile);
    outFile.close();

    Logger::getInstance().log(String("enable_BLE set to: ") + (enable_BLE ? "true" : "false"));
    Logger::getInstance().log(String("router_autostart set to: ") + (enable_BLE ? "false" : "true"));

    ESP.restart();
}
void SpecialAction::toggleAP(bool toggle)
{
    // Nuovo metodo per accendere in modo utile AP
    if (!FileSystemManager::ensureMounted())
    {
        Logger::getInstance().log("Failed to initialize LittleFS");
        return;
    }

    File configFile = LittleFS.open("/config.json", "r");
    if (!configFile)
    {

        Logger::getInstance().log("Failed to open config file");
        return;
    }

    size_t size = configFile.size();
    if (size > 4096)
    {
        Logger::getInstance().log("Config file size is too large");
        configFile.close();
        return;
    }

    std::unique_ptr<char[]> buf(new char[size]);
    configFile.readBytes(buf.get(), size);
    configFile.close();

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, buf.get());
    if (error)
    {
        Logger::getInstance().log(String("Failed to parse config file") + String(error.c_str()));
        return;
    }

    doc["system"]["ap_autostart"] = toggle;

    // **Salva le modifiche nel file**
    File outFile = LittleFS.open("/config.json", "w");
    if (!outFile)
    {
        Logger::getInstance().log("Failed to open config file for writing");
        return;
    }

    serializeJson(doc, outFile);
    outFile.close();

    Logger::getInstance().log(String("ap_autostart set to: ") + (toggle ? "false" : "true"));
}

void SpecialAction::executeGesture(bool pressed)
{
    if (!configManager.getAccelerometerConfig().active || !inputHub.hasGestureSensor())
    {
        Logger::getInstance().log("Accelerometer disabled");
        return;
    }

    if (pressed)
    {
        if (inputHub.startGestureCapture())
        {
            Logger::getInstance().log("Execution started - make your gesture");
        }
        else
        {
            Logger::getInstance().log("Execution already running");
        }
    }
    else
    {
        if (!inputHub.stopGestureCapture())
        {
            Logger::getInstance().log("Execution stop requested but gesture capture inactive");
            return;
        }

        int gestureID = inputHub.getLastGestureId();
        String gestureName = inputHub.getLastGestureName();

        if (gestureID < 0 && gestureName.length() == 0)
        {
            Logger::getInstance().log("No gesture recognized");
            return;
        }

        String message = "Recognized gesture";
        if (gestureName.length() > 0)
        {
            message += ": ";
            message += gestureName;
        }
        if (gestureID >= 0)
        {
            message += " (G_ID:";
            message += String(gestureID);
            message += ")";
        }
        Logger::getInstance().log(message);
    }
}
// ==================== IR REMOTE CONTROL FUNCTIONS ====================
// Toggle pattern: activation starts immediate scan, waits for IR signal, then saves

void SpecialAction::toggleScanIR(int deviceId, const String &exitCombo)
{
    vTaskDelay(pdMS_TO_TICKS(10));

    IRSensor *irSensor = inputHub.getIrSensor();
    IRStorage *irStorage = inputHub.getIrStorage();
    Keypad *keypadDevice = inputHub.getKeypad();

    if (!irSensor || !irStorage)
    {
        Logger::getInstance().log("IR Sensor or Storage not initialized");
        return;
    }

    // Power management: register activity
    powerManager.registerActivity();

    static bool scanningMode = false;
    static int currentDeviceId = -1;
    static String deviceName = "";
    static int savedRed = 0, savedGreen = 0, savedBlue = 0; // Save LED state

    // If already scanning with same device, exit scan mode
    if (scanningMode && currentDeviceId == deviceId)
    {
        Logger::getInstance().log("Exiting IR Scan mode for DEV" + String(deviceId));
        scanningMode = false;
        currentDeviceId = -1;
        currentLedMode = LedMode::NONE;
        // Pulisce buffer IR quando si esce
        irSensor->clearBuffer();
        // Restore LED color
        Led::getInstance().setColor(savedRed, savedGreen, savedBlue, false);
        return;
    }

    // If scanning with a different device ID, switch to the new one
    if (scanningMode && currentDeviceId != deviceId)
    {
        Logger::getInstance().log("Switching from DEV" + String(currentDeviceId) + " to DEV" + String(deviceId));
    }

    // Save current LED color
    Led::getInstance().getColor(savedRed, savedGreen, savedBlue);

    // Enter scan mode and start immediate capture
    scanningMode = true;
    currentLedMode = LedMode::IR_SCAN;
    currentDeviceId = deviceId;
    deviceName = "dev" + String(deviceId);

    // IMPORTANTE: Pulisce buffer IR prima di iniziare la scansione
    irSensor->clearBuffer();

    String exitMsg = "IR Scan DEV" + String(deviceId) + " ACTIVE - Point remote and press button now!";
    if (exitCombo.length() > 0)
    {
        exitMsg += " (Press " + exitCombo + " to exit)";
    }
    Logger::getInstance().log(exitMsg);
    Logger::getInstance().processBuffer();

    // Wait for IR signal with timeout
    unsigned long startTime = millis();
    unsigned long lastBlinkTime = millis();
    const unsigned long scanTimeout = 10000; // 10 seconds to capture signal
    const unsigned long blinkInterval = 500; // Slow blink: 500ms
    bool ledState = false;
    bool signalCaptured = false;
    bool exitRequested = false;

    // Track pressed keys for exit combo detection
    uint16_t pressedKeys = 0;
    uint16_t lastPressedKeys = 0;

    while (millis() - startTime < scanTimeout && scanningMode && !exitRequested)
    {
        // Slow red blink
        if (millis() - lastBlinkTime >= blinkInterval)
        {
            ledState = !ledState;
            if (ledState)
            {
                Led::getInstance().setColor(255, 0, 0, false); // Red ON
            }
            else
            {
                Led::getInstance().setColor(0, 0, 0, false); // LED OFF
            }
            lastBlinkTime = millis();
        }

        // Check for exit combo input
        if (exitCombo.length() > 0 && keypadDevice && keypadDevice->processInput())
        {
            InputEvent event = keypadDevice->getEvent();
            if (event.type == InputEvent::EventType::KEY_PRESS)
            {
                if (event.state) // Key pressed
                {
                    pressedKeys |= (1 << event.value1);
                    lastPressedKeys = pressedKeys;
                }
                else // Key released
                {
                    pressedKeys &= ~(1 << event.value1);

                    // When all keys are released, check what was pressed
                    if (pressedKeys == 0 && lastPressedKeys != 0)
                    {
                        // Build the combo string from lastPressedKeys
                        String pressedCombo = "";
                        for (int i = 0; i < 9; i++)
                        {
                            if (lastPressedKeys & (1 << i))
                            {
                                if (pressedCombo.length() > 0) pressedCombo += "+";
                                pressedCombo += String(i + 1);
                            }
                        }

                        // Check if it matches the exit combo
                        if (pressedCombo == exitCombo)
                        {
                            Logger::getInstance().log("Exit combo detected - cancelling scan");
                            exitRequested = true;
                            lastPressedKeys = 0;
                            break;
                        }

                        lastPressedKeys = 0;
                    }
                }
            }
        }

        if (irSensor->checkAndDecodeSignal())
        {
            signalCaptured = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Check every 10ms for better responsiveness
    }

    // Handle exit cases: no signal captured OR exit combo pressed
    if (!signalCaptured || exitRequested)
    {
        if (!signalCaptured && !exitRequested)
        {
            Logger::getInstance().log("No IR signal captured - scan cancelled");
        }
        scanningMode = false;
        currentDeviceId = -1;
        currentLedMode = LedMode::NONE;
        // Pulisce buffer IR quando si esce
        irSensor->clearBuffer();
        // Restore LED color
        Led::getInstance().setColor(savedRed, savedGreen, savedBlue, false);
        return;
    }

    // Signal captured - get the results
    const decode_results &res = irSensor->getRawSignalObject();

    if (res.value != 0 || res.rawlen > 0)
    {
        // Lampeggiamento rosso/verde per segnalare cattura segnale
        const unsigned long celebrationDuration = 1000; // 1 secondo di lampeggiamento
        const unsigned long celebrationInterval = 100; // Cambio colore ogni 100ms
        unsigned long celebrationStart = millis();
        bool greenRed = false;

        while (millis() - celebrationStart < celebrationDuration)
        {
            if ((millis() - celebrationStart) % celebrationInterval < (celebrationInterval / 2))
            {
                Led::getInstance().setColor(255, 0, 0, false); // Rosso
            }
            else
            {
                Led::getInstance().setColor(0, 255, 0, false); // Verde
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        Logger::getInstance().log("IR captured! Press key (1-9), combo (e.g., 1+2), gesture, or encoder (CW/CCW/BUTTON) to name it");
        Logger::getInstance().processBuffer();

        String commandName = getInputWithEncoder(15000, false, true, exitCombo); // Allow keypad, gesture, and encoder
        if (commandName == "")
        {
            Logger::getInstance().log("Timeout or invalid input - IR not saved");
            scanningMode = false;
            currentDeviceId = -1;
            currentLedMode = LedMode::NONE;
            // Pulisce buffer IR quando si esce
            irSensor->clearBuffer();
            return;
        }

        // Check if user pressed exit combo
        if (commandName == "EXIT_COMBO")
        {
            Logger::getInstance().log("Exit combo pressed - cancelling IR save");
            scanningMode = false;
            currentDeviceId = -1;
            currentLedMode = LedMode::NONE;
            // Pulisce buffer IR quando si esce
            irSensor->clearBuffer();
            return;
        }

        // Safety check: prevent saving exitCombo as a command name
        if (commandName == exitCombo)
        {
            Logger::getInstance().log("Cannot save exit combo as IR command - choose different combo");
            scanningMode = false;
            currentDeviceId = -1;
            currentLedMode = LedMode::NONE;
            // Pulisce buffer IR quando si esce
            irSensor->clearBuffer();
            return;
        }

        // Try to save as decoded command first
        bool saved = false;
        if (res.value != 0 && res.decode_type != UNKNOWN)
        {
            saved = irStorage->addIRCommand(deviceName, commandName,
                                            res.decode_type, res.value, res.bits);
        }

        // If decoded save failed or unknown protocol, save as RAW
        if (!saved && res.rawlen > 0)
        {
            uint16_t rawLen = 0;
            uint16_t *rawArray = irSensor->getRawDataSimple(rawLen);

            if (rawArray != nullptr && rawLen > 0)
            {
                if (rawLen > 128)
                {
                    Logger::getInstance().log("Signal truncated to 128 elements");
                    rawLen = 128;
                }
                saved = irStorage->addRawIRCommand(deviceName, commandName, rawArray, rawLen);
                delete[] rawArray;
            }
        }

        if (saved && irStorage->saveIRData())
        {
            Logger::getInstance().log("Saved: " + deviceName + "/" + commandName);
        }
        else
        {
            Logger::getInstance().log("Failed to save IR command");
        }
    }
    else
    {
        Logger::getInstance().log("No valid IR signal detected");
    }

    scanningMode = false;
    currentDeviceId = -1;
    currentLedMode = LedMode::NONE;
    // Pulisce buffer IR quando si esce
    irSensor->clearBuffer();
    // Restore LED color
    Led::getInstance().setColor(savedRed, savedGreen, savedBlue, false);
    powerManager.registerActivity();
}

void SpecialAction::toggleSendIR(int deviceId, const String &exitCombo)
{
    vTaskDelay(pdMS_TO_TICKS(10));

    IRSender *irSender = inputHub.getIrSender();
    IRStorage *irStorage = inputHub.getIrStorage();

    if (!irSender || !irStorage)
    {
        Logger::getInstance().log("IR Sender or Storage not initialized");
        return;
    }

    if (!irSender->isEnabled())
    {
        Logger::getInstance().log("IR Sender disabled");
        return;
    }

    // Power management: register activity
    powerManager.registerActivity();

    static bool sendingMode = false;
    static int currentDeviceId = -1;
    static String deviceName = "";
    static int savedRed = 0, savedGreen = 0, savedBlue = 0; // Save LED state

    // If already in sending mode with the same device ID, exit sending mode
    if (sendingMode && currentDeviceId == deviceId)
    {
        Logger::getInstance().log("Exiting IR Send mode for DEV" + String(deviceId));
        sendingMode = false;
        currentDeviceId = -1;
        currentLedMode = LedMode::NONE;
        // Restore LED color
        Led::getInstance().setColor(savedRed, savedGreen, savedBlue, false);
        return;
    }

    // If sending with a different device ID, switch to the new one
    if (sendingMode && currentDeviceId != deviceId)
    {
        Logger::getInstance().log("Switching from DEV" + String(currentDeviceId) + " to DEV" + String(deviceId));
    }

    // Save current LED color
    Led::getInstance().getColor(savedRed, savedGreen, savedBlue);

    // Enter or switch sending mode
    sendingMode = true;
    currentLedMode = LedMode::IR_SEND;
    currentDeviceId = deviceId;
    deviceName = "dev" + String(deviceId);

    // Set LED to red when entering send mode
    Led::getInstance().setColor(255, 0, 0, false);

    String exitMsg = "IR Send DEV" + String(deviceId) + " ACTIVE - Press key, combo, gesture, or encoder to send";
    if (exitCombo.length() > 0)
    {
        exitMsg += " (Press " + exitCombo + " to exit)";
    }
    Logger::getInstance().log(exitMsg);
    Logger::getInstance().processBuffer();

    // Loop to continuously send commands until mode is exited
    while (sendingMode && currentDeviceId == deviceId)
    {
        Logger::getInstance().log("Ready to send - select command (key/combo/gesture/encoder)...");
        Logger::getInstance().processBuffer();

        String commandName = getInputWithEncoder(5000, false, true, exitCombo); // Allow keypad, gesture, and encoder
        if (commandName == "")
        {
            Logger::getInstance().log("Timeout - waiting for next input or exit");
            powerManager.registerActivity();
            continue;
        }

        // Check if user pressed exit combo
        if (commandName == "EXIT_COMBO")
        {
            Logger::getInstance().log("Exit combo pressed - exiting send mode");
            sendingMode = false;
            currentDeviceId = -1;
            currentLedMode = LedMode::NONE;
            // Restore LED color
            Led::getInstance().setColor(savedRed, savedGreen, savedBlue, false);
            break;
        }

        JsonObject commandObj = irStorage->getCommand(deviceName, commandName);
        if (commandObj.isNull())
        {
            Logger::getInstance().log("Not found: " + deviceName + "/" + commandName);
            continue;
        }

        // Fast blink during IR send
        unsigned long sendStartTime = millis();
        const unsigned long fastBlinkInterval = 100; // Fast blink: 100ms
        bool blinkLedState = false;

        // Start fast blink
        while (millis() - sendStartTime < 100)
        {
            if ((millis() - sendStartTime) % fastBlinkInterval < (fastBlinkInterval / 2))
            {
                Led::getInstance().setColor(255, 0, 0, false); // Red ON
            }
            else
            {
                Led::getInstance().setColor(0, 0, 0, false); // LED OFF
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        if (irSender->sendCommand(commandObj))
        {
            Logger::getInstance().log("Sent: " + deviceName + "/" + commandName);
        }
        else
        {
            Logger::getInstance().log("Failed to send IR");
        }

        // Restore red LED after sending
        Led::getInstance().setColor(255, 0, 0, false);

        powerManager.registerActivity();
    }

    // Restore LED color when exiting loop
    currentLedMode = LedMode::NONE;
    Led::getInstance().setColor(savedRed, savedGreen, savedBlue, false);
}

void SpecialAction::sendIRCommand(const String &deviceName, const String &commandName)
{
    IRSender *irSender = inputHub.getIrSender();
    IRStorage *irStorage = inputHub.getIrStorage();

    if (!irSender || !irStorage)
    {
        Logger::getInstance().log("IR Sender or Storage not initialized");
        return;
    }

    if (!irSender->isEnabled())
    {
        Logger::getInstance().log("IR Sender disabled");
        return;
    }

    // Power management: register activity
    powerManager.registerActivity();

    JsonObject commandObj = irStorage->getCommand(deviceName, commandName);
    if (commandObj.isNull())
    {
        Logger::getInstance().log("IR cmd not found: " + deviceName + "/" + commandName);
        return;
    }

    // Save current LED state
    int savedRed, savedGreen, savedBlue;
    Led::getInstance().getColor(savedRed, savedGreen, savedBlue);

    // Fast blink during IR send
    unsigned long sendStartTime = millis();
    const unsigned long fastBlinkInterval = 100; // Fast blink: 100ms
    const unsigned long blinkDuration = 200; // Blink for 200ms total

    // Send command with fast blink effect
    bool commandSent = false;
    while (millis() - sendStartTime < blinkDuration)
    {
        if ((millis() - sendStartTime) % fastBlinkInterval < (fastBlinkInterval / 2))
        {
            Led::getInstance().setColor(255, 0, 0, false); // Red ON
        }
        else
        {
            Led::getInstance().setColor(0, 0, 0, false); // LED OFF
        }

        // Send command at the beginning
        if (!commandSent)
        {
            if (irSender->sendCommand(commandObj))
            {
                Logger::getInstance().log("IR sent: " + deviceName + "/" + commandName);
            }
            else
            {
                Logger::getInstance().log("Failed to send IR: " + deviceName + "/" + commandName);
            }
            commandSent = true;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Restore LED color
    Led::getInstance().setColor(savedRed, savedGreen, savedBlue, false);
}

void SpecialAction::checkIRSignal()
{
    IRSensor *irSensor = inputHub.getIrSensor();
    IRSender *irSender = inputHub.getIrSender();
    IRStorage *irStorage = inputHub.getIrStorage();

    if (!irSensor)
    {
        Logger::getInstance().log("IR Sensor not initialized");
        return;
    }

    // Print IR Settings
    Logger::getInstance().log("=== IR Settings ===");
    Logger::getInstance().log("IR Sensor: " + String(irSensor ? "Initialized" : "Not initialized"));
    Logger::getInstance().log("IR Sender: " + String(irSender ? (irSender->isEnabled() ? "Enabled" : "Disabled") : "Not initialized"));
    
    // Print stored IR data
    if (irStorage)
    {
        Logger::getInstance().log("=== IR Data (Stored Commands) ===");
        String jsonData = irStorage->getJsonString();
        
        // Log raw JSON
        Logger::getInstance().log("Raw JSON: " + jsonData);
        
        if (jsonData.length() > 0 && jsonData != "{\"devices\":{}}")
        {
            // Parse and display in a readable format
            const JsonDocument& doc = irStorage->getJsonObject();
            JsonObjectConst devices = doc["devices"];
            
            if (devices.size() > 0)
            {
                for (JsonPairConst devicePair : devices)
                {
                    String deviceName = devicePair.key().c_str();
                    JsonObjectConst commands = devicePair.value();
                    
                    Logger::getInstance().log("Device: " + deviceName + " (" + String(commands.size()) + " commands)");
                    
                    for (JsonPairConst cmdPair : commands)
                    {
                        String cmdName = cmdPair.key().c_str();
                        JsonObjectConst cmdData = cmdPair.value();
                        
                        String cmdInfo = "  - " + cmdName + ": ";
                        if (cmdData.containsKey("protocol"))
                        {
                            cmdInfo += "Protocol=" + String(cmdData["protocol"].as<const char*>());
                        }
                        if (cmdData.containsKey("value"))
                        {
                            cmdInfo += " Value=0x" + String(cmdData["value"].as<const char*>());
                        }
                        if (cmdData.containsKey("bits"))
                        {
                            cmdInfo += " Bits=" + String(cmdData["bits"].as<int>());
                        }
                        if (cmdData.containsKey("raw"))
                        {
                            JsonArrayConst raw = cmdData["raw"];
                            cmdInfo += " Raw[" + String(raw.size()) + "]";
                        }
                        
                        Logger::getInstance().log(cmdInfo);
                    }
                }
            }
            else
            {
                Logger::getInstance().log("No devices stored");
            }
        }
        else
        {
            Logger::getInstance().log("No IR data stored");
        }
    }
    else
    {
        Logger::getInstance().log("IR Storage not initialized");
    }
    
    // Check for incoming IR signal
    Logger::getInstance().log("=== Checking for IR Signal ===");
    if (irSensor->checkAndDecodeSignal())
    {
        const decode_results &rawSignal = irSensor->getRawSignalObject();

        String output = "IR: 0x" + String(rawSignal.value, HEX);
        output += " Proto=" + irSensor->getProtocolName(rawSignal.decode_type);
        output += " Bits=" + String(rawSignal.bits);
        output += " Len=" + String(rawSignal.rawlen);

        Logger::getInstance().log(output);

        // Show first 20 raw values
        if (rawSignal.rawlen > 0)
        {
            String rawData = "Raw: ";
            uint16_t maxShow = (rawSignal.rawlen < 20) ? rawSignal.rawlen : 20;
            for (uint16_t i = 0; i < maxShow; i++)
            {
                rawData += String(rawSignal.rawbuf[i]);
                if (i < maxShow - 1)
                    rawData += ",";
            }
            if (rawSignal.rawlen > 20)
                rawData += "...";
            Logger::getInstance().log(rawData);
        }
    }
    else
    {
        Logger::getInstance().log("No IR signal detected");
    }
}

// ==================== LED RGB CONTROL FUNCTIONS ====================

void SpecialAction::setLedColor(int red, int green, int blue, bool save)
{
    // Manual LED control - NO brightness applied
    red = constrain(red, 0, 255);
    green = constrain(green, 0, 255);
    blue = constrain(blue, 0, 255);

    if (Led::getInstance().setColor(red, green, blue, save))
    {
        Logger::getInstance().log("LED set to RGB(" + String(red) + "," + String(green) + "," + String(blue) + ") [manual]");
    }
}

void SpecialAction::setSystemLedColor(int red, int green, int blue, bool save)
{
    // System notification LED control - brightness applied
    red = constrain(red, 0, 255);
    green = constrain(green, 0, 255);
    blue = constrain(blue, 0, 255);

    // Salva i valori RGB originali (prima dello scaling)
    originalRed = red;
    originalGreen = green;
    originalBlue = blue;

    if (save)
    {
        savedSystemRed = red;
        savedSystemGreen = green;
        savedSystemBlue = blue;
        systemColorSaved = true;
    }

    const bool reactiveOwnsLed = reactiveLightingActive && currentLedMode == LedMode::REACTIVE;
    if (reactiveOwnsLed)
    {
        // Reactive lighting owns the LED - store values for later restore and schedule color restore
        const bool colorChanged = !systemColorDeferred ||
                                  deferredSystemRed != red ||
                                  deferredSystemGreen != green ||
                                  deferredSystemBlue != blue;

        if (colorChanged || (save && !deferredSystemSave))
        {
            deferredSystemRed = red;
            deferredSystemGreen = green;
            deferredSystemBlue = blue;
            systemColorDeferred = true;
            deferredSystemSave = deferredSystemSave || save;

            if (colorChanged)
            {
                deferredSystemLogged = false;
            }

            if (save && !deferredSystemLogged)
            {
                Logger::getInstance().log("LED system color update deferred (reactive lighting active)");
                deferredSystemLogged = true;
            }
        }

        constexpr unsigned long REACTIVE_RESTORE_DELAY_MS = 600;
        inputHub.scheduleReactiveLightingRestore(REACTIVE_RESTORE_DELAY_MS);
    }
    else
    {
        systemColorDeferred = false;
        deferredSystemSave = false;
        deferredSystemLogged = false;
    }

    // Apply brightness scaling (0-255)
    float brightnessScale = currentBrightness / 255.0f;
    int adjustedRed = (int)(red * brightnessScale);
    int adjustedGreen = (int)(green * brightnessScale);
    int adjustedBlue = (int)(blue * brightnessScale);

    if (Led::getInstance().setColor(adjustedRed, adjustedGreen, adjustedBlue, save))
    {
        Logger::getInstance().log("LED set to RGB(" + String(red) + "," + String(green) + "," + String(blue) +
                                  ") @ " + String(currentBrightness) + "/255 brightness [system]");
    }
}

void SpecialAction::adjustLedColor(int redDelta, int greenDelta, int blueDelta)
{
    // Get current LED color
    int currentRed, currentGreen, currentBlue;
    Led::getInstance().getColor(currentRed, currentGreen, currentBlue);

    // Apply deltas with clamping
    int newRed = constrain(currentRed + redDelta, 0, 255);
    int newGreen = constrain(currentGreen + greenDelta, 0, 255);
    int newBlue = constrain(currentBlue + blueDelta, 0, 255);

    if (Led::getInstance().setColor(newRed, newGreen, newBlue, false))
    {
        Logger::getInstance().log("LED adjusted from RGB(" + String(currentRed) + "," + String(currentGreen) + "," + String(currentBlue) +
                                  ") to RGB(" + String(newRed) + "," + String(newGreen) + "," + String(newBlue) + ")");
    }
}

void SpecialAction::turnOffLed()
{
    if (Led::getInstance().setColor(0, 0, 0, false))
    {
        Logger::getInstance().log("LED turned OFF");
    }
}

void SpecialAction::saveLedColor()
{
    // Get current color and save it
    int red, green, blue;
    Led::getInstance().getColor(red, green, blue);
    Led::getInstance().setColor(red, green, blue, true);
    Logger::getInstance().log("LED color saved: RGB(" + String(red) + "," + String(green) + "," + String(blue) + ")");
}

void SpecialAction::restoreLedColor()
{
    if (Led::getInstance().setColor(true)) // Restore saved color
    {
        int red, green, blue;
        Led::getInstance().getColor(red, green, blue);
        Logger::getInstance().log("LED color restored: RGB(" + String(red) + "," + String(green) + "," + String(blue) + ")");
    }
    else
    {
        Logger::getInstance().log("No saved LED color available for restore");
    }
}

bool SpecialAction::isIrModeActive() const
{
    return currentLedMode == LedMode::IR_SCAN || currentLedMode == LedMode::IR_SEND;
}

void SpecialAction::setReactiveLightingActive(bool active)
{
    if (active)
    {
        if (!reactiveLightingActive)
        {
            reactiveLightingActive = true;
            if (currentLedMode == LedMode::NONE)
            {
                currentLedMode = LedMode::REACTIVE;
            }
        }
        return;
    }

    reactiveLightingActive = false;
    if (currentLedMode == LedMode::REACTIVE)
    {
        currentLedMode = LedMode::NONE;
    }

    if (systemColorDeferred)
    {
        applyDeferredSystemLedColor();
    }
}

void SpecialAction::applyDeferredSystemLedColor()
{
    if (!systemColorDeferred)
    {
        return;
    }

    const int red = deferredSystemRed;
    const int green = deferredSystemGreen;
    const int blue = deferredSystemBlue;
    const bool save = deferredSystemSave;

    systemColorDeferred = false;
    deferredSystemSave = false;
    deferredSystemLogged = false;

    setSystemLedColor(red, green, blue, save);
}

void SpecialAction::saveSystemLedColor()
{
    // Save the ORIGINAL (unscaled) RGB values + brightness
    savedSystemRed = originalRed;
    savedSystemGreen = originalGreen;
    savedSystemBlue = originalBlue;
    systemColorSaved = true;
    Logger::getInstance().log("System LED color saved: RGB(" + String(originalRed) + "," + String(originalGreen) + "," + String(originalBlue) + ") @ " + String(currentBrightness) + "/255");
}

void SpecialAction::restoreSystemLedColor()
{
    if (currentLedMode != LedMode::NONE)
    {
        // A special mode is active, don't restore the system color
        return;
    }

    if (!systemColorSaved)
    {
        // No saved color - just reapply current original values with brightness
        // This happens on first call before any save was made
        setSystemLedColor(originalRed, originalGreen, originalBlue, true);
        return;
    }

    // Restore by reapplying brightness to the saved original values
    setSystemLedColor(savedSystemRed, savedSystemGreen, savedSystemBlue, true);
}

void SpecialAction::showLedInfo()
{
    int red, green, blue;
    Led::getInstance().getColor(red, green, blue);
    String colorInfo = "LED: RGB(" + String(red) + "," + String(green) + "," + String(blue) + ") - " + Led::getInstance().getColorLog();
    colorInfo += " @ " + String(currentBrightness) + "/255 brightness";
    Logger::getInstance().log(colorInfo);
}

// ==================== LED BRIGHTNESS CONTROL FUNCTIONS ====================

void SpecialAction::saveBrightnessToFile()
{
    const uint8_t persisted = static_cast<uint8_t>(constrain(currentBrightness, 0, 255));
    if (!configManager.setLedBrightness(persisted))
    {
        Logger::getInstance().log("Failed to persist LED brightness to config.json");
    }
}

void SpecialAction::loadBrightness()
{
    const LedConfig &ledConfig = configManager.getLedConfig();
    const int configured = static_cast<int>(ledConfig.brightness);
    currentBrightness = constrain(configured, 0, 255);
    Logger::getInstance().log("Loaded brightness from config: " + String(currentBrightness));
}

void SpecialAction::setBrightness(int brightness)
{
    const int oldBrightness = currentBrightness;
    currentBrightness = constrain(brightness, 0, 255);

    // Refresh current LED color with new brightness (only if brightness changed)
    if (currentBrightness != oldBrightness)
    {
        // Usa i valori RGB originali salvati e applica il nuovo brightness
        float newScale = currentBrightness / 255.0f;
        int newRed = (int)(originalRed * newScale);
        int newGreen = (int)(originalGreen * newScale);
        int newBlue = (int)(originalBlue * newScale);

        // Applica il nuovo colore con il nuovo brightness
        Led::getInstance().setColor(newRed, newGreen, newBlue, false);

        saveBrightnessToFile();
    }

    Logger::getInstance().log("Brightness set to " + String(currentBrightness) + "/255 (applies to system notifications only)");
}

void SpecialAction::adjustBrightness(int delta)
{
    const int previous = currentBrightness;
    setBrightness(currentBrightness + delta);

    Logger::getInstance().log("Brightness adjusted from " + String(previous) +
                              " to " + String(currentBrightness) + "/255");
}

int SpecialAction::getBrightness()
{
    return currentBrightness;
}

void SpecialAction::showBrightnessInfo()
{
    Logger::getInstance().log("LED Brightness: " + String(currentBrightness) + "/255");
}

void SpecialAction::toggleFlashlight()
{
    if (!flashlightActive)
    {
        // Save current LED state
        Led::getInstance().getColor(flashlightSavedColor[0], flashlightSavedColor[1], flashlightSavedColor[2]);
        // Turn on white LED at full brightness
        Led::getInstance().setColor(255, 255, 255, false);
        flashlightActive = true;
        currentLedMode = LedMode::FLASHLIGHT;
        Logger::getInstance().log("Flashlight ON - LED set to white (255,255,255)");
    }
    else
    {
        // Restore previous LED state
        Led::getInstance().setColor(flashlightSavedColor[0], flashlightSavedColor[1], flashlightSavedColor[2], false);
        flashlightActive = false;
        currentLedMode = LedMode::NONE;
        Logger::getInstance().log("Flashlight OFF - LED restored to RGB(" +
                                String(flashlightSavedColor[0]) + "," +
                                String(flashlightSavedColor[1]) + "," +
                                String(flashlightSavedColor[2]) + ")");
    }
}
