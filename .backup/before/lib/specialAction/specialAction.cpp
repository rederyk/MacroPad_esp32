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
#include <gestureStorage.h>
#include <LittleFS.h>
#include "keypad.h"
#include "Logger.h"
#include "powerManager.h"
#include "IRSensor.h"
#include "IRSender.h"
#include "IRStorage.h"
#include <ArduinoJson.h>

extern Keypad *keypad;
extern IRSensor *irSensor;
extern IRSender *irSender;
extern IRStorage *irStorage;

extern GestureRead gestureSensor;
extern GestureAnalyze gestureAnalyzer;
extern GestureStorage gestureStorage;
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

    while (key < 0 || key > 8)
    {
        if (keypad->processInput())
        {
            InputEvent event = keypad->getEvent();
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
            return -1;
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Dai tempo al logger
    }
    return key;
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
    if (!configManager.getAccelerometerConfig().active)
    {
        Logger::getInstance().log("Accelerometer disabled");
        return "";
    }
    static unsigned long executeTime = 0;

    if (gestureSensor.isSampling())
    {
        return ""; // No gesture recognized yet
    }
    else
    {
        // Get gesture ID using KNN
        int gestureID = gestureAnalyzer.findKNNMatch(6); // Use 6 nearest neighbors
        String result = "G_ID:";
        if (gestureID >= 0)
        {
            // Format return string
            result += String(gestureID);
        }
        else
        {
            result = "";
        }
        return result;
    }
}

void SpecialAction::toggleSampling(bool pressed)
{
    if (!configManager.getAccelerometerConfig().active)
    {
        Logger::getInstance().log("Accelerometer disabled");
        return;
    }
    if (pressed)
    {
        if (!gestureSensor.isSampling())
        {
            gestureSensor.startSampling();
            Logger::getInstance().log("Sampling started");
        }
        else
        {
            Logger::getInstance().log("Sampling already started");
        }
    }
    else
    {
        if (gestureSensor.isSampling())
        {
            gestureSensor.stopSampling();
            Logger::getInstance().log("Sampling stopped");
        }
        else
        {
            Logger::getInstance().log("Sampling already stopped");
        }
    }
}

bool SpecialAction::saveGesture(int id)
{
    if (!configManager.getAccelerometerConfig().active)
    {
        Logger::getInstance().log("Accelerometer disabled");
        return false;
    }
    // Basic ID validation inline
    if (id < 0 || id > 8)
    {
        Logger::getInstance().log("Invalid ID (0-8 only)");
        return false;
    }

    return gestureAnalyzer.saveFeaturesWithID(id);
}

bool SpecialAction::convertJsonToBinary()
{
    size_t numGestures, numSamples, numFeatures;
    float ***matrix = gestureStorage.convertJsonToMatrix3D(numGestures, numSamples, numFeatures);

    if (!matrix)
        return false;

    bool success = gestureStorage.saveMatrixToBinary(
        "/gestures.bin",
        matrix,
        numGestures,
        numSamples,
        numFeatures);

    // Memory cleanup handled by saveMatrixToBinary() (DONT_REMOVE_THIS)
    return success;
}

void SpecialAction::clearAllGestures()
{
    gestureStorage.clearGestureFeatures();
}

void SpecialAction::printJson()
{
    gestureStorage.printJsonFeatures();
    return;
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
    if (LittleFS.begin())
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

    if (LittleFS.begin())
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
    if (!LittleFS.begin(true))
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
    if (!LittleFS.begin(true))
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

void SpecialAction::clearGestureWithID(int key)
{
    Logger::getInstance().log("Press key 1-9 to delete gesture");

    key = getKeypadInput(5000);

    if (key < 0 || key > 8)
    {
        return;
    }

    if (gestureStorage.clearGestureFeatures(key))
    {
        Logger::getInstance().log(String("Gesture ") + String(key).c_str() + String(" deleted"));
    }
    else
    {
        Logger::getInstance().log("Failed to delete gesture");
    }
}

void SpecialAction::executeGesture(bool pressed)
{
    if (!configManager.getAccelerometerConfig().active)
    {
        Logger::getInstance().log("Accelerometer disabled");
        return;
    }
    static unsigned long executeTime = 0;

    // Toggle sampling state
    toggleSampling(pressed);
    // Stop execution mode and recognize
    executeTime = millis();

    if (gestureSensor.isSampling())
    {
        // Start execution mode
        Logger::getInstance().log("Execution started - make your gesture");
        return; // No gesture recognized yet
    }
    else if (!gestureSensor.isSampling() || millis() - executeTime > 5000)
    {
        // Get gesture ID using KNN
        int gestureID = gestureAnalyzer.findKNNMatch(6); // Use 3 nearest neighbors

        // Format return string
        String result = "gesture_ID:";
        result += String(gestureID);

        Logger::getInstance().log(String("Recognized gesture ID: ") + result.c_str());

        return;
    }
}
void SpecialAction::trainGesture(bool pressed, int key)
{
    if (!configManager.getAccelerometerConfig().active)
    {
        Logger::getInstance().log("Accelerometer disabled");
        return;
    }
    // Toggle sampling state
    toggleSampling(pressed);

    if (gestureSensor.isSampling())
    {
        // Start training mode
        Logger::getInstance().log("Training started - make your gesture");
    }
    else
    {
        // Stop training mode and save
        Logger::getInstance().log("Press key 1-9 to save gesture");
        Logger::getInstance().processBuffer();

        key = getKeypadInput(5000);

        if (key < 0 || key > 8)
        {
            Logger::getInstance().log("Training timeout");
            return;
        }

        // Save gesture with ID (0-8)
        if (saveGesture(key))
        {
            Logger::getInstance().log(String("Gesture saved with ID: ") + String(key).c_str());
        }
        else
        {
            Logger::getInstance().log("Failed to save gesture");
        }
    }
}

// ==================== IR REMOTE CONTROL FUNCTIONS ====================

void SpecialAction::learnIRCommand(int repeats)
{
    if (!irSensor || !irStorage)
    {
        Logger::getInstance().log("IR Sensor or Storage not initialized");
        return;
    }

    // Power management: register activity
    powerManager.registerActivity();

    Logger::getInstance().log("IR Learn Mode - Press device key (1-9):");
    Logger::getInstance().log("1=TV 2=AC 3=Light 4=Fan 5=Audio");
    Logger::getInstance().processBuffer();

    int deviceKey = getKeypadInput(10000);
    if (deviceKey < 0 || deviceKey > 8)
    {
        Logger::getInstance().log("Timeout or invalid device key");
        return;
    }

    // Map key to device names
    const char* deviceNames[] = {"tv", "ac", "light", "fan", "audio", "dev6", "dev7", "dev8", "dev9"};
    String device = deviceNames[deviceKey];

    Logger::getInstance().log("Device: " + device);
    Logger::getInstance().log("Press command key (1-9):");
    Logger::getInstance().log("1=Power 2=Up 3=Down 4=OK 5=Back");
    Logger::getInstance().processBuffer();

    int commandKey = getKeypadInput(10000);
    if (commandKey < 0 || commandKey > 8)
    {
        Logger::getInstance().log("Timeout or invalid command key");
        return;
    }

    // Map key to command names
    const char* commandNames[] = {"power", "up", "down", "ok", "back", "cmd6", "cmd7", "cmd8", "cmd9"};
    String command = commandNames[commandKey];

    Logger::getInstance().log("Command: " + command);
    Logger::getInstance().log("Press IR button " + String(repeats) + " times...");
    Logger::getInstance().processBuffer();

    // Intercept the IR command
    decode_results res = irSensor->interceptCommand(repeats, true);

    if (res.value != 0)
    {
        Logger::getInstance().log("IR captured: 0x" + String(res.value, HEX));

        if (irStorage->addIRCommand(device, command, res.decode_type, res.value, res.bits))
        {
            if (irStorage->saveIRData())
            {
                Logger::getInstance().log("Command saved: " + device + "/" + command);
            }
            else
            {
                Logger::getInstance().log("Failed to save to file");
            }
        }
        else
        {
            Logger::getInstance().log("Failed to add command");
        }
    }
    else
    {
        Logger::getInstance().log("No valid IR signal detected");
    }

    // Register activity after operation
    powerManager.registerActivity();
}

void SpecialAction::saveLastIRRaw()
{
    if (!irSensor || !irStorage)
    {
        Logger::getInstance().log("IR Sensor or Storage not initialized");
        return;
    }

    // Power management: register activity
    powerManager.registerActivity();

    const decode_results &rawSignal = irSensor->getRawSignalObject();

    if (rawSignal.rawlen == 0)
    {
        Logger::getInstance().log("No IR signal detected. Use checkIRSignal first");
        return;
    }

    Logger::getInstance().log("Save RAW - Press device key (1-9):");
    Logger::getInstance().processBuffer();

    int deviceKey = getKeypadInput(10000);
    if (deviceKey < 0 || deviceKey > 8)
    {
        Logger::getInstance().log("Timeout or invalid key");
        return;
    }

    const char* deviceNames[] = {"tv", "ac", "light", "fan", "audio", "dev6", "dev7", "dev8", "dev9"};
    String device = deviceNames[deviceKey];

    Logger::getInstance().log("Press command key (1-9):");
    Logger::getInstance().processBuffer();

    int commandKey = getKeypadInput(10000);
    if (commandKey < 0 || commandKey > 8)
    {
        Logger::getInstance().log("Timeout or invalid key");
        return;
    }

    const char* commandNames[] = {"power", "up", "down", "ok", "back", "cmd6", "cmd7", "cmd8", "cmd9"};
    String command = commandNames[commandKey];

    // Get raw data
    uint16_t rawLen = 0;
    uint16_t* rawArray = irSensor->getRawDataSimple(rawLen);

    if (rawArray == nullptr || rawLen == 0)
    {
        Logger::getInstance().log("Failed to convert RAW data");
        return;
    }

    if (rawLen > 128)
    {
        Logger::getInstance().log("Signal too long, truncating to 128");
        rawLen = 128;
    }

    if (irStorage->addRawIRCommand(device, command, rawArray, rawLen))
    {
        if (irStorage->saveIRData())
        {
            Logger::getInstance().log("RAW saved: " + device + "/" + command);
        }
        else
        {
            Logger::getInstance().log("Failed to save to file");
        }
    }
    else
    {
        Logger::getInstance().log("Failed to add RAW command");
    }

    delete[] rawArray;
    powerManager.registerActivity();
}

void SpecialAction::sendIRCommand()
{
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

    Logger::getInstance().log("Send IR - Press device key (1-9):");
    Logger::getInstance().processBuffer();

    int deviceKey = getKeypadInput(10000);
    if (deviceKey < 0 || deviceKey > 8)
    {
        Logger::getInstance().log("Timeout or invalid key");
        return;
    }

    const char* deviceNames[] = {"tv", "ac", "light", "fan", "audio", "dev6", "dev7", "dev8", "dev9"};
    String device = deviceNames[deviceKey];

    Logger::getInstance().log("Press command key (1-9):");
    Logger::getInstance().processBuffer();

    int commandKey = getKeypadInput(10000);
    if (commandKey < 0 || commandKey > 8)
    {
        Logger::getInstance().log("Timeout or invalid key");
        return;
    }

    const char* commandNames[] = {"power", "up", "down", "ok", "back", "cmd6", "cmd7", "cmd8", "cmd9"};
    String command = commandNames[commandKey];

    JsonObject commandObj = irStorage->getCommand(device, command);
    if (commandObj.isNull())
    {
        Logger::getInstance().log("Command not found: " + device + "/" + command);
        return;
    }

    if (irSender->sendCommand(commandObj))
    {
        Logger::getInstance().log("IR sent: " + device + "/" + command);
    }
    else
    {
        Logger::getInstance().log("Failed to send IR command");
    }

    powerManager.registerActivity();
}

void SpecialAction::getLastIRCommand()
{
    if (!irSensor)
    {
        Logger::getInstance().log("IR Sensor not initialized");
        return;
    }

    if (irSensor->checkAndDecodeSignal())
    {
        unsigned long lastCommand = irSensor->getRawSignalObject().value;
        if (lastCommand != 0)
        {
            Logger::getInstance().log("Last IR: 0x" + String(lastCommand, HEX));
        }
        else
        {
            Logger::getInstance().log("No IR command detected");
        }
    }
    else
    {
        Logger::getInstance().log("No IR signal available");
    }
}

void SpecialAction::checkIRSignal()
{
    if (!irSensor)
    {
        Logger::getInstance().log("IR Sensor not initialized");
        return;
    }

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
                if (i < maxShow - 1) rawData += ",";
            }
            if (rawSignal.rawlen > 20) rawData += "...";
            Logger::getInstance().log(rawData);
        }
    }
    else
    {
        Logger::getInstance().log("No IR signal detected");
    }
}
