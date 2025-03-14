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
        Led::getInstance().setColor(255, 0, 255); // Magenta
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
    powerManager.begin(configManager.getSystemConfig(), configManager.getKeypadConfig());

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
    // TODO make optional
    const AccelerometerConfig &accelConfig = configManager.getAccelerometerConfig();
    if (accelConfig.active)
    {
        Wire.begin(accelConfig.sdaPin, accelConfig.sclPin);

        // Start the sensor
        if (!gestureSensor.begin())
        {
            Logger::getInstance().log("Could not find ADXL345 sensor. Check wiring!");
            // TODO rendere acceleroemtro opzionale

            while (1)
                ;
        }
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

        bleController.storeOriginalMAC();
        //        Keyboard.deviceName = systemConfig.BleName.c_str();

        int mac = systemConfig.BleMacAdd;
        bleController.incrementMacAddress(mac);
        delay(20);
        bleController.incrementName(mac);
        delay(20);
        bleController.startBluetooth();
        delay(20);

        Logger::getInstance().log("Free heap after Bluetooth start: " + String(ESP.getFreeHeap()) + " bytes");
        Led::getInstance().setColor(0, 0, 255); // Blu
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

                delay(wifiConnectTimeout / 2); // Wait for 5s before checking again
                Logger::getInstance().log("Checking WiFi connection...");
            }

            // If not connected after the timeout, start the AP
            if (!wifiManager.isConnected())

            {
                Logger::getInstance().log("Failed to connect to STA_MODE_WiFi. ");
                if (!systemConfig.ap_autostart)
                {

                    Led::getInstance().setColor(255, 0, 0); // Rosso
                    Logger::getInstance().log("LED acceso: " + Led::getInstance().getColorLog(), true);
                    Logger::getInstance().log("Starting AP BACKUP MODE...");
                    wifiManager.beginAP(configManager.getWifiConfig().ap_ssid.c_str(), configManager.getWifiConfig().ap_password.c_str());
                }
            }
            else
            {
                Led::getInstance().setColor(0, 255, 0); // Verde
                Logger::getInstance().log("LED acceso: " + Led::getInstance().getColorLog(), true);
                Logger::getInstance().log("connesso a " + configManager.getWifiConfig().router_ssid);
            }
        }

        if (systemConfig.ap_autostart)
        {
            Led::getInstance().setColor(255, 0, 0); // Rosso
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
    static unsigned long lastUpdateTime = 0;
    const unsigned long updateInterval = 1; // 1ms base interval

    for (;;)
    {
        unsigned long now = millis();

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
            // Registra attività utente
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
            // Registra attività utente
            powerManager.registerActivity();
        }

        // Process buffered events
        processEvents();
        bleController.checkConnection();

        // Update gesture sampling
        gestureSensor.updateSampling();

        // Update macro manager
        macroManager.update();

        // Controlla inattività per sleep mode (solo in BLE mode)
        if (powerManager.checkInactivity())
        {
            Logger::getInstance().log("Inactivity detected, entering sleep mode...");

            // Svuota il buffer del logger
            Logger::getInstance().processBuffer();

            // Entra in sleep mode
            powerManager.enterDeepSleep();
            // powerManager.enterDeepSleepWithMultipleWakeupPins();
        }

        Logger::getInstance().processBuffer();
        // Maintain consistent timing
        if (now - lastUpdateTime < updateInterval)
        {
            delay(updateInterval - (now - lastUpdateTime));
        }
        lastUpdateTime = now;
    }
}

void loop()
{
    // Empty - all processing happens in mainLoopTask
}