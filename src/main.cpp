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

#include <Arduino.h>
#include <queue>
#include <esp_system.h>
#include <esp_err.h>
#include <esp_log.h>

#include <ArduinoJson.h>
#include "Logger.h"
#include "Led.h"
#include "configManager.h"
#include "combinationManager.h"
#include "keypad.h"
#include "rotaryEncoder.h"
#include "BLEController.h"
#include "macroManager.h"
#include "inputDevice.h"
#include "gestureRead.h"
#include "gestureAnalyze.h"
#include "WIFIManager.h"
#include "specialAction.h"
#include "powerManager.h"
#include "IRSensor.h"
#include "IRSender.h"
#include "IRStorage.h"

WIFIManager wifiManager; // Create an instance of WIFIManager

// Event buffer structure
struct TimedEvent
{
    InputEvent event;
    unsigned long timestamp;
};

ConfigurationManager configManager;
PowerManager powerManager;
CombinationManager comboManager;
GestureRead gestureSensor; // Definizione effettiva (deve rimanere UNICA)
GestureAnalyze gestureAnalyzer(gestureSensor);
SpecialAction specialAction;
BLEController bleController("Macropad_esp32"); // prendere nome dal config in qualche modo...uguale per wifi e bluethoot
// modificare blecontroller.start??
MacroManager macroManager(nullptr, nullptr);
Keypad *keypad;
RotaryEncoder *rotaryEncoder;

// IR Remote Control components (pointers for optional initialization)
IRSensor *irSensor = nullptr;
IRSender *irSender = nullptr;
IRStorage *irStorage = nullptr;

// Event buffer
std::queue<TimedEvent> eventBuffer;
const size_t MAX_BUFFER_SIZE = 16;

// Function to process buffered events
void processEvents()
{
    while (!eventBuffer.empty())
    {
        TimedEvent timedEvent = eventBuffer.front();
        eventBuffer.pop();

        // Handle event in macro manager
        macroManager.handleInputEvent(timedEvent.event);
    }
}

// Task function prototype
void mainLoopTask(void *parameter);

void setup()
{

    // Ottieni l'istanza del Logger
    Logger &logger = Logger::getInstance();

    logger.log("🔹 Logger avviato correttamente!");

    // Load configuration first
    if (!configManager.loadConfig())
    {
        logger.setSerialEnabled(true);
        logger.log("Failed to load configuration from json! forced enable serial true");
        while (true)
            ;
    }

    const LedConfig ledConfig = configManager.getLedConfig();
    if (ledConfig.active)
    {
        Led::getInstance().begin(ledConfig.pinRed, ledConfig.pinGreen, ledConfig.pinBlue, ledConfig.anodeCommon);

        // Load saved brightness before setting any colors
        specialAction.loadBrightness();

        specialAction.setSystemLedColor(255, 0, 255, false); // Magenta with loaded brightness (system notification)
        Logger::getInstance().log("LED acceso: " + Led::getInstance().getColorLog(), true);
    }
    // Set serial enabled based on config

    bool serialEnabled = configManager.getSystemConfig().serial_enabled;
    if (serialEnabled)
    {
        Serial.begin(115200);
    }

    logger.setSerialEnabled(serialEnabled);
    // Inizializza il PowerManager con la configurazione
    powerManager.begin(configManager.getSystemConfig(), configManager.getKeypadConfig(), configManager.getEncoderConfig());

    // Load combinations
    if (!comboManager.loadCombinations(configManager.getSystemConfig().BleMacAdd))
    {
        logger.log("Failed to load combinations");
    }
    // Initialize macroManager with keypad config and wifi config

    Logger::getInstance().log("\nESP32 Keypad and Encoder Test");
    macroManager = MacroManager(
        &configManager.getKeypadConfig(),
        &configManager.getWifiConfig());

    // Load combinations into macroManager
    {
        JsonObject combos = comboManager.getCombinations();
        // some debug
        //  String debugCombos;
        //  serializeJson(combos, debugCombos);
        //  Logger::getInstance().log("Combos: " + debugCombos);

        for (JsonPair combo : combos)
        {
            std::vector<std::string> actions;
            for (JsonVariant v : combo.value().as<JsonArray>())
            {
                actions.push_back(std::string(v.as<const char *>()));
            }
            macroManager.combinations[std::string(combo.key().c_str())] = actions;
        }
    }

    // Internal loaded combinations
    Logger::getInstance().log("Loaded " + String(macroManager.combinations.size()) + " combinations");

    // Initialize I2C with configured pins

    const AccelerometerConfig &accelConfig = configManager.getAccelerometerConfig();
    if (accelConfig.active)
    {
        Wire.begin(accelConfig.sdaPin, accelConfig.sclPin);

        String accelType = accelConfig.type.length() > 0 ? accelConfig.type : "adxl345";
        accelType.toLowerCase();
        String message = "Initialising accelerometer type: " + accelType;
        if (accelConfig.address)
        {
            message += " (0x";
            message += String(accelConfig.address, HEX);
            message += ")";
        }
        Logger::getInstance().log(message);

        // Start the sensor
        if (!gestureSensor.begin(accelConfig))
        {
            Logger::getInstance().log("Accelerometer init failed, continuing without gesture support.");
        }
        else
        {
            Logger::getInstance().log("Accelerometer initialised successfully.");
            if (gestureSensor.calibrate())
            {
                Logger::getInstance().log("Accelerometer calibration completed at startup.");
            }
            else
            {
                Logger::getInstance().log("Accelerometer calibration failed at startup.");
            }
        }
    }

    // Initialize IR Remote Control components (ONLY when BLE is disabled)
    // REASON: IRremoteESP8266 conflicts with BLE due to timer/RMT hardware sharing and memory constraints
    const SystemConfig &irSystemConfig = configManager.getSystemConfig();
    if (!irSystemConfig.enable_BLE)
    {
        Logger::getInstance().log("BLE disabled - IR components will be initialized");

        const IRSensorConfig &irSensorConfig = configManager.getIrSensorConfig();
        Logger::getInstance().log("IR Sensor Config: pin=" + String(irSensorConfig.pin) +
                                  ", active=" + String(irSensorConfig.active ? "true" : "false"));
        if (irSensorConfig.active && irSensorConfig.pin >= 0)
        {
            Logger::getInstance().log("Initializing IR Sensor on pin " + String(irSensorConfig.pin));
            irSensor = new IRSensor(irSensorConfig.pin);
            if (irSensor && irSensor->begin())
            {
                Logger::getInstance().log("IR Sensor initialized successfully");
            }
            else
            {
                Logger::getInstance().log("Failed to initialize IR Sensor");
                if (irSensor)
                {
                    delete irSensor;
                    irSensor = nullptr;
                }
            }
        }
        else
        {
            Logger::getInstance().log("IR Sensor NOT initialized (disabled or invalid pin)");
        }

        const IRLedConfig &irLedConfig = configManager.getIrLedConfig();
        Logger::getInstance().log("IR LED Config: pin=" + String(irLedConfig.pin) +
                                  ", active=" + String(irLedConfig.active ? "true" : "false") +
                                  ", anodeGpio=" + String(irLedConfig.anodeGpio ? "true" : "false"));
        if (irLedConfig.active && irLedConfig.pin >= 0)
        {
            Logger::getInstance().log("Initializing IR Sender on pin " + String(irLedConfig.pin));
            irSender = new IRSender(irLedConfig.pin, irLedConfig.anodeGpio);
            if (irSender && irSender->begin())
            {
                Logger::getInstance().log("IR Sender initialized successfully");
            }
            else
            {
                Logger::getInstance().log("Failed to initialize IR Sender");
                if (irSender)
                {
                    delete irSender;
                    irSender = nullptr;
                }
            }
        }
        else
        {
            Logger::getInstance().log("IR Sender NOT initialized (disabled or invalid pin)");
        }

        // Initialize IR Storage (always initialize if either sensor or sender is active)
        Logger::getInstance().log("Checking IR Storage initialization: irSensor=" +
                                  String(irSensor != nullptr ? "OK" : "NULL") +
                                  ", irSender=" + String(irSender != nullptr ? "OK" : "NULL"));
        if ((irSensor != nullptr) || (irSender != nullptr))
        {
            Logger::getInstance().log("Initializing IR Storage");
            irStorage = new IRStorage();
            if (irStorage && irStorage->begin())
            {
                Logger::getInstance().log("IR Storage initialized successfully");
                if (irStorage->loadIRData())
                {
                    Logger::getInstance().log("IR data loaded from file");
                }
                else
                {
                    Logger::getInstance().log("No existing IR data file (this is normal on first run)");
                }
            }
            else
            {
                Logger::getInstance().log("Failed to initialize IR Storage (LittleFS error?)");
                if (irStorage)
                {
                    delete irStorage;
                    irStorage = nullptr;
                }
            }
        }
        else
        {
            Logger::getInstance().log("IR Storage NOT initialized (no IR sensor or sender available)");
        }

        Logger::getInstance().log("Free heap after IR initialization: " + String(ESP.getFreeHeap()) + " bytes");
    }
    else
    {
        Logger::getInstance().log("BLE enabled - IR components DISABLED to avoid memory/hardware conflicts");
        Logger::getInstance().log("To use IR: disable BLE in config.json (enable_BLE: false) and restart");
    }

    // Initialize hardware with configurations
    keypad = new Keypad(&configManager.getKeypadConfig());
    rotaryEncoder = new RotaryEncoder(&configManager.getEncoderConfig());

    keypad->setup();
    rotaryEncoder->setup();

    // Create main loop task with sufficient stack size
    xTaskCreateUniversal(
        mainLoopTask,   // Task function
        "mainLoopTask", // Task name
        //  32768,          // 32KB stack size
        16384, // 16KB stack size
        NULL,  // Parameters
        2,     // Priority (same as gesture task)
        NULL,  // Task handle,
        CONFIG_ARDUINO_RUNNING_CORE);

    // Log free RAM memory
    Logger::getInstance().log("Free RAM memory: " + String(ESP.getFreeHeap()) + " bytes");

    // Log free flash memory
    Logger::getInstance().log("Free sketch memory: " + String(ESP.getFreeSketchSpace()) + " bytes");

    /// system Feature
    // Get system config
    const SystemConfig &systemConfig = configManager.getSystemConfig();

    // autostart stuff
    if (systemConfig.enable_BLE)
    {
        // Release Bluetooth Classic memory (frees ~30KB for BLE/IR compatibility)
        Logger::getInstance().log("Releasing Bluetooth Classic memory...");
        esp_err_t ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
        if (ret == ESP_OK)
        {
            Logger::getInstance().log("Bluetooth Classic memory released successfully");
        }
        else
        {
            Logger::getInstance().log("Failed to release BT Classic memory: " + String(ret));
        }
        Logger::getInstance().log("Free heap after BT Classic release: " + String(ESP.getFreeHeap()) + " bytes");

        bleController.storeOriginalMAC();
        //        Keyboard.deviceName = systemConfig.BleName.c_str();

        int mac = systemConfig.BleMacAdd;
        bleController.incrementMacAddress(mac);
        vTaskDelay(pdMS_TO_TICKS(50)); // Dai tempo al BLE
        bleController.incrementName(mac);
        vTaskDelay(pdMS_TO_TICKS(50)); // Dai tempo al BLE
        bleController.startBluetooth();
        vTaskDelay(pdMS_TO_TICKS(50)); // Dai tempo al BLE

        Logger::getInstance().log("Free heap after Bluetooth start: " + String(ESP.getFreeHeap()) + " bytes");
        specialAction.setSystemLedColor(0, 0, 255, false); // Blu with saved brightness (BLE notification)
        Logger::getInstance().log("LED acceso: " + Led::getInstance().getColorLog(), true);
    }
    else
    {

        // Conditionally start STA

        if (systemConfig.router_autostart)
        {
            Logger::getInstance().log("Starting STA mode...");

            wifiManager.connectWiFi(configManager.getWifiConfig().router_ssid.c_str(), configManager.getWifiConfig().router_password.c_str());

            // Define the timeout duration (in milliseconds)
            // funziona senza?
            const unsigned long wifiConnectTimeout = 5000; // 5 seconds

            // Check WiFi connection for the specified duration
            unsigned long startTime = millis();

            while (!wifiManager.isConnected() && (millis() - startTime < wifiConnectTimeout))
            {

                // delay(wifiConnectTimeout / 2); // Wait for 5s before checking again
                vTaskDelay(pdMS_TO_TICKS(wifiConnectTimeout / 2)); // Wait for 5s before checking again

                Logger::getInstance().log("Checking WiFi connection...");
            }

            // If not connected after the timeout, start the AP
            if (!wifiManager.isConnected())

            {
                Logger::getInstance().log("Failed to connect to STA_MODE_WiFi. ");
                if (!systemConfig.ap_autostart)
                {

                    specialAction.setSystemLedColor(255, 0, 0, false); // Rosso with saved brightness (WiFi failed notification)
                    Logger::getInstance().log("LED acceso: " + Led::getInstance().getColorLog(), true);
                    Logger::getInstance().log("Starting AP BACKUP MODE...");
                    wifiManager.beginAP(configManager.getWifiConfig().ap_ssid.c_str(), configManager.getWifiConfig().ap_password.c_str());
                }
            }
            else
            {
                specialAction.setSystemLedColor(0, 255, 0, false); // Verde with saved brightness (WiFi connected notification)
                Logger::getInstance().log("LED acceso: " + Led::getInstance().getColorLog(), true);
                Logger::getInstance().log("connesso a " + configManager.getWifiConfig().router_ssid);
            }
        }

        if (systemConfig.ap_autostart)
        {
            specialAction.setSystemLedColor(255, 0, 0, false); // Rosso with saved brightness (AP mode notification)
            Logger::getInstance().log("LED acceso: " + Led::getInstance().getColorLog(), true);
            Logger::getInstance().log("Starting AP mode...");
            wifiManager.beginAP(configManager.getWifiConfig().ap_ssid.c_str(), configManager.getWifiConfig().ap_password.c_str());
        }

        Logger::getInstance().log("Free heap before webserver start: " + String(ESP.getFreeHeap()) + " bytes");
    }

    Logger::getInstance().log("Hardware initialized with name ");

    Logger::getInstance().log("Press keys or rotate encoder to test...");
}

void mainLoopTask(void *parameter)
{
    // Definisci la frequenza desiderata in tick
    // Prova con un intervallo iniziale, ad esempio 1ms
    const TickType_t xFrequency = pdMS_TO_TICKS(5); // 1 ms = 1000 µs
    TickType_t xLastWakeTime;

    // Inizializza xLastWakeTime con il tempo corrente PRIMA di entrare nel loop
    xLastWakeTime = xTaskGetTickCount(); // Ottiene il conteggio attuale dei tick

    // Variabili per tracciare il tempo massimo
    long maxExecutionTimeMicros = 0;              // Tempo massimo nel periodo di log
    const unsigned long logIntervalMillis = 5000; // Logga il massimo ogni 5 secondi
    unsigned long lastLogTime = millis();

    Logger::getInstance().log("mainLoopTask started. Target interval: " + String(pdTICKS_TO_MS(xFrequency)) + " ms. Logging max execution time every " + String(logIntervalMillis) + " ms.");

    for (;;)
    {
        // --- Inizio Misurazione ---
        int64_t startTimeMicros = esp_timer_get_time();

        // ----- INIZIO del tuo codice del loop -----
        // Metti qui TUTTO il codice che vuoi eseguire ad ogni ciclo
        unsigned long now = millis(); // Necessario per i timestamp degli eventi

        // Check for keypad events
        if (keypad->processInput())
        {
            TimedEvent timedEvent;
            timedEvent.event = keypad->getEvent();
            timedEvent.timestamp = now;
            if (eventBuffer.size() < MAX_BUFFER_SIZE)
            {
                eventBuffer.push(timedEvent);
            }
            powerManager.registerActivity();
        }

        // Check for encoder events
        if (rotaryEncoder->processInput())
        {
            TimedEvent timedEvent;
            timedEvent.event = rotaryEncoder->getEvent();
            timedEvent.timestamp = now;
            if (eventBuffer.size() < MAX_BUFFER_SIZE)
            {
                eventBuffer.push(timedEvent);
            }
            powerManager.registerActivity();
        }

        // Process buffered events
        processEvents();


        // --- Operazioni potenzialmente più lunghe ---
        bleController.checkConnection();
        gestureSensor.updateSampling(); // Assicurati che non blocchi
        macroManager.update();          // Assicurati che non blocchi

        // Controlla inattività per sleep mode
        if (powerManager.checkInactivity())
        {
            Logger::getInstance().log("Inactivity detected, entering sleep mode...");
            Logger::getInstance().processBuffer(); // Svuota prima di dormire
            vTaskDelay(pdMS_TO_TICKS(50));         // Dai tempo al logger
            powerManager.enterDeepSleep();
            // Non ritorna da deep sleep qui
        }

        // Invio log accumulati (può richiedere tempo se buffer pieno)
        Logger::getInstance().processBuffer();
        // ----- FINE del tuo codice del loop -----
    /*
        // --- Fine Misurazione ---
        int64_t endTimeMicros = esp_timer_get_time();
        long executionTimeMicros = (long)(endTimeMicros - startTimeMicros);

        // Aggiorna il tempo massimo riscontrato *in questo intervallo di log*
        if (executionTimeMicros > maxExecutionTimeMicros)
        {
            maxExecutionTimeMicros = executionTimeMicros;
        }

    
          // --- Log Periodico del Massimo ---
           unsigned long currentTime = millis();
           if (currentTime - lastLogTime >= logIntervalMillis)
           {
               // Logga il tempo massimo trovato negli ultimi 'logIntervalMillis'
               Logger::getInstance().log("Max loop exec time (last " + String(logIntervalMillis / 1000.0, 1) + "s): " + String(maxExecutionTimeMicros) + " µs");

               // Resetta il massimo per trovare il picco nel prossimo intervallo
               maxExecutionTimeMicros = 0;
               lastLogTime = currentTime; // Aggiorna il tempo dell'ultimo log
           }
     */

        // Attendi fino al prossimo momento di attivazione calcolato
        // Questo cede il controllo allo scheduler fino al prossimo intervallo
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void loop()
{
    // Empty - all processing happens in mainLoopTask
}
