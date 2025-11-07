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

#include "configWebServer.h"
#include "SpecialActionRouter.h"
#include "EventScheduler.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <algorithm>
#include <cctype>
#include <vector>
#include "Logger.h"
#include "FileSystemManager.h"
#include "InputHub.h"
#include "IRStorage.h"
#include "IRSensor.h"
#include "Led.h"
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <time.h>

extern InputHub inputHub;
extern EventScheduler eventScheduler;

AsyncWebServer server(80);
AsyncEventSource events("/log");

// IR Scan background mode variables
static bool irScanModeActive = false;
static unsigned long irScanStartTime = 0;
static unsigned long irScanLastBlinkTime = 0;
static bool irScanLedState = false;
static int irScanSavedRed = 0;
static int irScanSavedGreen = 0;
static int irScanSavedBlue = 0;
static const unsigned long IR_SCAN_TIMEOUT = 60000; // 60 secondi timeout
static const unsigned long IR_SCAN_BLINK_INTERVAL = 500; // 500ms blink

struct SpecialActionDescriptor
{
    const char *id;
    const char *label;
    const char *endpoint;
    const char *method;
    const char *description;
    bool requiresParams;
    const char *examplePayload;
    const char *actionId;
};

static const SpecialActionDescriptor kSpecialActions[] = {
    {"reset_device", "Riavvio dispositivo", "/resetDevice", "POST", "Esegue un riavvio immediato dell'ESP32.", false, nullptr, nullptr},
    {"calibrate_sensor", "Calibra sensore", "/calibrateSensor", "POST", "Avvia la routine di calibrazione dell'accelerometro.", false, nullptr, nullptr},
    {"print_memory_info", "Stato memoria", "/special_action", "POST", "Invia ai log le informazioni sull'utilizzo di heap e memoria.", false, "{\"actionId\":\"print_memory_info\"}", "print_memory_info"},
    {"execute_gesture", "Esegui gesture", "/special_action", "POST", "Avvia o termina la cattura gesture in base al flag 'pressed'.", true, "{\"actionId\":\"execute_gesture\",\"params\":{\"pressed\":true}}", "execute_gesture"},
    {"toggle_flashlight", "Toggle flashlight", "/special_action", "POST", "Attiva o disattiva il LED come torcia, mantenendo il colore precedente.", false, "{\"actionId\":\"toggle_flashlight\"}", "toggle_flashlight"},
    {"toggle_ir_scan", "Toggle IR scan", "/special_action", "POST", "Attiva o disattiva la modalità scansione IR per acquisizione codici da remoto.", true, "{\"actionId\":\"toggle_ir_scan\",\"params\":{\"active\":true}}", "toggle_ir_scan"},
    {"set_led_color", "Imposta colore LED", "/special_action", "POST", "Aggiorna il colore RGB principale del LED di stato.", true, "{\"actionId\":\"set_led_color\",\"params\":{\"r\":255,\"g\":128,\"b\":64,\"save\":false}}", "set_led_color"},
    {"set_system_led_color", "Imposta colore sistema", "/special_action", "POST", "Imposta il colore del LED di sistema e opzionalmente lo salva come default.", true, "{\"actionId\":\"set_system_led_color\",\"params\":{\"r\":32,\"g\":128,\"b\":255,\"save\":true}}", "set_system_led_color"},
    {"restore_led_color", "Ripristina colore LED", "/special_action", "POST", "Ripristina il colore originale del LED salvato in precedenza.", false, "{\"actionId\":\"restore_led_color\"}", "restore_led_color"},
    {"set_brightness", "Imposta luminosità", "/special_action", "POST", "Imposta la luminosità del LED (0-255) e la salva su config.json.", true, "{\"actionId\":\"set_brightness\",\"params\":{\"value\":180}}", "set_brightness"},
    {"adjust_brightness", "Regola luminosità", "/special_action", "POST", "Incrementa/decrementa la luminosità attuale del LED.", true, "{\"actionId\":\"adjust_brightness\",\"params\":{\"delta\":15}}", "adjust_brightness"},
    {"show_led_info", "Mostra info LED", "/special_action", "POST", "Scrive nei log il colore corrente e la luminosità del LED.", false, "{\"actionId\":\"show_led_info\"}", "show_led_info"},
    {"show_brightness_info", "Mostra luminosità", "/special_action", "POST", "Scrive nei log il livello di luminosità corrente del LED.", false, "{\"actionId\":\"show_brightness_info\"}", "show_brightness_info"},
    {"check_ir_signal", "Verifica segnale IR", "/special_action", "POST", "Verifica rapidamente la presenza di un segnale IR e lo riporta nei log.", false, "{\"actionId\":\"check_ir_signal\"}", "check_ir_signal"},
    {"send_ir_command", "Invia comando IR", "/special_action", "POST", "Invia un comando IR memorizzato specificando dispositivo e comando.", true, "{\"actionId\":\"send_ir_command\",\"params\":{\"device\":\"tv\",\"command\":\"off\"}}", "send_ir_command"}};

String readIrDataFile()
{
    File file = LittleFS.open("/ir_data.json", "r");
    if (!file)
    {
        Logger::getInstance().log("⚠️ Failed to open ir_data.json");
        return "{\"devices\":{}}";
    }

    String json = file.readString();
    file.close();

    if (json.length() == 0)
    {
        Logger::getInstance().log("⚠️ ir_data.json is empty!");
        return "{\"devices\":{}}";
    }

    return json;
}

bool writeIrDataFile(const String &json)
{
    File file = LittleFS.open("/ir_data.json", "w");
    if (!file)
    {
        Logger::getInstance().log("⚠️ Failed to open ir_data.json for writing.");
        return false;
    }

    size_t written = file.print(json);
    file.close();
    Logger::getInstance().log("💾 Saved ir_data.json (" + String(written) + " bytes)");
    return written > 0;
}

// Funzione per convertire decode_results in JSON string
String irDecodeToJson(const decode_results &results)
{
    StaticJsonDocument<512> doc;

    // Protocol
    String protocol = typeToString(results.decode_type, false);
    doc["protocol"] = protocol;

    // Bits
    doc["bits"] = results.bits;

    // Value (hex senza 0x prefix)
    char valueHex[20];
    if (results.bits <= 32) {
        snprintf(valueHex, sizeof(valueHex), "%llx", (unsigned long long)results.value);
    } else {
        snprintf(valueHex, sizeof(valueHex), "%llx", (unsigned long long)results.value);
    }
    doc["value"] = valueHex;

    // Address e command (se disponibili)
    if (results.address != 0 || results.decode_type == NEC || results.decode_type == SAMSUNG) {
        doc["address"] = results.address;
    }
    if (results.command != 0 || results.decode_type == NEC || results.decode_type == SAMSUNG) {
        doc["command"] = results.command;
    }

    // Repeat flag
    doc["repeat"] = results.repeat;

    String output;
    serializeJson(doc, output);
    return output;
}

// Check IR in background mode - viene chiamato dal loop
void checkIRScanBackground()
{
    if (!irScanModeActive) return;

    unsigned long currentMillis = millis();

    // Timeout check
    if (currentMillis - irScanStartTime > IR_SCAN_TIMEOUT) {
        Logger::getInstance().log("[IR SCAN] Timeout - stopping scan mode");
        irScanModeActive = false;
        Led::getInstance().setColor(irScanSavedRed, irScanSavedGreen, irScanSavedBlue, false);
        return;
    }

    // LED blink (rosso lampeggiante)
    if (currentMillis - irScanLastBlinkTime >= IR_SCAN_BLINK_INTERVAL) {
        irScanLedState = !irScanLedState;
        if (irScanLedState) {
            Led::getInstance().setColor(255, 0, 0, false); // Red ON
        } else {
            Led::getInstance().setColor(0, 0, 0, false); // LED OFF
        }
        irScanLastBlinkTime = currentMillis;
    }

    // Check for IR signal
    IRSensor *irSensor = inputHub.getIrSensor();
    if (irSensor && irSensor->checkAndDecodeSignal()) {
        const decode_results &results = irSensor->getRawSignalObject();

        // Ignora segnali repeat se non necessari
        if (results.repeat) {
            return;
        }

        // Converti in JSON
        String jsonOutput = irDecodeToJson(results);

        // Invia al logger con prefisso IR:
        Logger::getInstance().log("IR: " + jsonOutput);

        // Brief green flash per conferma acquisizione
        Led::getInstance().setColor(0, 255, 0, false);
        delay(100);
        Led::getInstance().setColor(irScanSavedRed, irScanSavedGreen, irScanSavedBlue, false);
    }
}

bool handleSpecialActionRequest(const String &actionId, JsonVariantConst params, String &message, int &statusCode)
{
    statusCode = 200;
    message = "Action executed.";

    if (actionId == "print_memory_info")
    {
        specialAction.printMemoryInfo();
        message = "Dump memoria richiesto nei log.";
        return true;
    }

    if (actionId == "execute_gesture")
    {
        bool pressed = false;
        if (!params.isNull())
        {
            pressed = params["pressed"] | false;
        }
        specialAction.executeGesture(pressed);
        message = pressed ? "Acquisizione gesture avviata." : "Acquisizione gesture terminata.";
        return true;
    }

    if (actionId == "toggle_flashlight")
    {
        specialAction.toggleFlashlight();
        message = "Flashlight toggled.";
        return true;
    }

    if (actionId == "set_led_color")
    {
        if (!params.is<JsonObjectConst>())
        {
            statusCode = 400;
            message = "Parametri mancanti per set_led_color.";
            return false;
        }
        JsonObjectConst obj = params.as<JsonObjectConst>();
        int red = obj["r"] | -1;
        int green = obj["g"] | -1;
        int blue = obj["b"] | -1;
        bool save = obj["save"] | false;
        if (red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 || blue > 255)
        {
            statusCode = 400;
            message = "Valori RGB fuori range (0-255).";
            return false;
        }
        specialAction.setLedColor(red, green, blue, save);
        message = "Colore LED aggiornato.";
        return true;
    }

    if (actionId == "set_system_led_color")
    {
        if (!params.is<JsonObjectConst>())
        {
            statusCode = 400;
            message = "Parametri mancanti per set_system_led_color.";
            return false;
        }
        JsonObjectConst obj = params.as<JsonObjectConst>();
        int red = obj["r"] | -1;
        int green = obj["g"] | -1;
        int blue = obj["b"] | -1;
        bool save = obj["save"] | false;
        if (red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 || blue > 255)
        {
            statusCode = 400;
            message = "Valori RGB fuori range (0-255).";
            return false;
        }
        specialAction.setSystemLedColor(red, green, blue, save);
        message = "Colore LED di sistema aggiornato.";
        return true;
    }

    if (actionId == "restore_led_color")
    {
        specialAction.restoreLedColor();
        message = "Colore LED ripristinato.";
        return true;
    }

    if (actionId == "set_brightness")
    {
        if (!params.is<JsonObjectConst>())
        {
            statusCode = 400;
            message = "Parametro 'value' richiesto.";
            return false;
        }
        int value = params["value"] | -1;
        if (value < 0 || value > 255)
        {
            statusCode = 400;
            message = "Brightness fuori range (0-255).";
            return false;
        }
        specialAction.setBrightness(value);
        message = "Luminosità impostata a " + String(value) + ".";
        return true;
    }

    if (actionId == "adjust_brightness")
    {
        if (!params.is<JsonObjectConst>())
        {
            statusCode = 400;
            message = "Parametro 'delta' richiesto.";
            return false;
        }
        int delta = params["delta"] | 0;
        if (delta == 0)
        {
            statusCode = 400;
            message = "Il delta deve essere diverso da 0.";
            return false;
        }
        specialAction.adjustBrightness(delta);
        message = "Luminosità regolata di " + String(delta) + ".";
        return true;
    }

    if (actionId == "show_led_info")
    {
        specialAction.showLedInfo();
        message = "Informazioni LED scritte nei log.";
        return true;
    }

    if (actionId == "show_brightness_info")
    {
        specialAction.showBrightnessInfo();
        message = "Informazioni luminosità scritte nei log.";
        return true;
    }

    if (actionId == "check_ir_signal")
    {
        specialAction.checkIRSignal();
        message = "Controllo segnale IR avviato.";
        return true;
    }

    if (actionId == "toggle_ir_scan")
    {
        bool active = false;
        if (!params.isNull() && params.is<JsonObjectConst>()) {
            active = params["active"] | false;
        }

        IRSensor *irSensor = inputHub.getIrSensor();
        if (!irSensor) {
            statusCode = 500;
            message = "IR Sensor not initialized.";
            return false;
        }

        if (active && !irScanModeActive) {
            // Attiva scan mode
            Led::getInstance().getColor(irScanSavedRed, irScanSavedGreen, irScanSavedBlue);
            irSensor->clearBuffer();
            irScanModeActive = true;
            irScanStartTime = millis();
            irScanLastBlinkTime = millis();
            irScanLedState = false;
            Logger::getInstance().log("[IR SCAN] Scan mode ACTIVATED - Point remote and press buttons");
            message = "IR scan mode activated";
        } else if (!active && irScanModeActive) {
            // Disattiva scan mode
            irScanModeActive = false;
            Led::getInstance().setColor(irScanSavedRed, irScanSavedGreen, irScanSavedBlue, false);
            Logger::getInstance().log("[IR SCAN] Scan mode DEACTIVATED");
            message = "IR scan mode deactivated";
        } else {
            message = active ? "IR scan already active" : "IR scan already inactive";
        }
        return true;
    }

    if (actionId == "send_ir_command")
    {
        if (!params.is<JsonObjectConst>())
        {
            statusCode = 400;
            message = "Parametri 'device' e 'command' richiesti.";
            return false;
        }
        JsonObjectConst obj = params.as<JsonObjectConst>();
        String device = obj["device"] | "";
        String command = obj["command"] | "";
        if (device.isEmpty() || command.isEmpty())
        {
            statusCode = 400;
            message = "Device o command mancanti.";
            return false;
        }
        specialAction.sendIRCommand(device, command);
        message = "Comando IR inviato a " + device + ":" + command;
        return true;
    }

    statusCode = 404;
    message = "Azione non supportata: " + actionId;
    return false;
}

// Funzioni helper per leggere/scrivere il file di configurazione
String readConfigFile()
{
    File file = LittleFS.open("/config.json", "r");
    if (!file)
    {
        Logger::getInstance().log("⚠️ Failed to open config.json");
        return "{}"; // Ritorna un oggetto JSON vuoto
    }
    String json = file.readString();
    Logger::getInstance().log("size ");
    Logger::getInstance().log(String(file.size()));
    file.close();
    if (json.length() == 0)
    {
        Logger::getInstance().log("⚠️ config.json is empty!");
        return "{}";
    }
    return json;
}

bool writeConfigFile(const String &json)
{
    File file = LittleFS.open("/config.json", "w");
    if (!file)
    {
        Logger::getInstance().log("⚠️ Failed to open config.json for writing.");
        return false;
    }
    file.print(json);
    file.close();
    return true;
}

// Funzioni helper per leggere/scrivere i file combo
String readComboFile()
{
    DynamicJsonDocument doc(32768);
    JsonObject root = doc.to<JsonObject>();

    File rootDir = LittleFS.open("/");
    if (!rootDir)
    {
        Logger::getInstance().log("⚠️ Failed to open LittleFS root while reading combos.");
        return "{}";
    }

    for (File entry = rootDir.openNextFile(); entry; entry = rootDir.openNextFile())
    {
        if (entry.isDirectory())
        {
            entry.close();
            continue;
        }

        String name = entry.name(); // e.g. "/combo_0.json"
        if (!name.startsWith("/combo_") || !name.endsWith(".json") || name == "/combo_common.json")
        {
            entry.close();
            continue;
        }

        String indexStr = name.substring(7, name.length() - 5); // between "/combo_" and ".json"
        bool isNumeric = !indexStr.isEmpty();
        for (size_t i = 0; i < indexStr.length(); ++i)
        {
            if (!isdigit(static_cast<unsigned char>(indexStr[i])))
            {
                isNumeric = false;
                break;
            }
        }

        if (!isNumeric)
        {
            Logger::getInstance().log("⚠️ Ignoring combo file with non-numeric suffix: " + name);
            entry.close();
            continue;
        }

        String rawContent = entry.readString();
        entry.close();

        if (rawContent.isEmpty())
        {
            Logger::getInstance().log("⚠️ Combo file " + name + " is empty.");
            continue;
        }

        DynamicJsonDocument setDoc(8192);
        DeserializationError error = deserializeJson(setDoc, rawContent);
        if (error)
        {
            Logger::getInstance().log("⚠️ Failed to parse " + name + ": " + String(error.c_str()));
            continue;
        }

        String setKey = "combinations_" + indexStr;
        root[setKey.c_str()] = setDoc.as<JsonVariant>();
    }

    rootDir.close();

    String output;
    serializeJson(root, output);

    if (output.length() == 0)
    {
        Logger::getInstance().log("⚠️ No combo files found.");
        return "{}";
    }

    Logger::getInstance().log("Combo file size: " + String(output.length()));
    return output;
}

bool writeComboFile(int setNumber, const String &json)
{
    String filePath = "/combo_" + String(setNumber) + ".json";
    File file = LittleFS.open(filePath.c_str(), "w");
    if (!file)
    {
        Logger::getInstance().log("⚠️ Failed to open " + filePath + " for writing.");
        return false;
    }
    file.print(json);
    file.close();
    Logger::getInstance().log("💾 Saved " + filePath);
    return true;
}

bool writeComboFile(const String &json)
{
    DynamicJsonDocument doc(32768);
    DeserializationError error = deserializeJson(doc, json);
    if (error)
    {
        Logger::getInstance().log("⚠️ Failed to parse JSON: " + String(error.c_str()));
        return false;
    }

    if (!doc.is<JsonObject>())
    {
        Logger::getInstance().log("⚠️ Combo payload is not a JSON object.");
        return false;
    }

    constexpr size_t prefixLen = 13; // strlen("combinations_")
    JsonObject root = doc.as<JsonObject>();
    std::vector<String> preservedFiles;
    bool success = true;

    for (JsonPair kv : root)
    {
        String key = kv.key().c_str();
        if (!key.startsWith("combinations_") || key.length() <= prefixLen)
        {
            Logger::getInstance().log("ℹ️ Ignoring non-standard combo key: " + key);
            continue;
        }

        String indexStr = key.substring(prefixLen);
        bool isNumeric = !indexStr.isEmpty();
        for (size_t i = 0; i < indexStr.length(); ++i)
        {
            if (!isdigit(static_cast<unsigned char>(indexStr[i])))
            {
                isNumeric = false;
                break;
            }
        }

        if (!isNumeric)
        {
            Logger::getInstance().log("⚠️ Ignoring combo key with non-numeric suffix: " + key);
            continue;
        }

        int setNumber = indexStr.toInt();
        String serialized;
        serializeJson(kv.value(), serialized);

        if (!writeComboFile(setNumber, serialized))
        {
            success = false;
        }

        preservedFiles.push_back("/combo_" + indexStr + ".json");
    }

    File rootDir = LittleFS.open("/");
    if (!rootDir)
    {
        Logger::getInstance().log("⚠️ Failed to open LittleFS root while cleaning combos.");
        return false;
    }

    for (File entry = rootDir.openNextFile(); entry; entry = rootDir.openNextFile())
    {
        if (entry.isDirectory())
        {
            entry.close();
            continue;
        }

        String name = entry.name();
        entry.close();

        if (!name.startsWith("/combo_") || !name.endsWith(".json") || name == "/combo_common.json")
        {
            continue;
        }

        if (std::find(preservedFiles.begin(), preservedFiles.end(), name) == preservedFiles.end())
        {
            if (LittleFS.remove(name))
            {
                Logger::getInstance().log("🗑️ Removed obsolete combo file " + name);
            }
            else
            {
                Logger::getInstance().log("⚠️ Failed to remove obsolete combo file " + name);
                success = false;
            }
        }
    }

    rootDir.close();
    return success;
}

bool isValidComboFilename(const String &name)
{
    if (name.length() == 0)
    {
        return false;
    }
    if (name.indexOf('/') != -1 || name.indexOf('\\') != -1 || name.indexOf("..") != -1)
    {
        return false;
    }
    if (!name.endsWith(".json"))
    {
        return false;
    }
    if (name.startsWith("combo_") || name.startsWith("my_combo_") || name == "combo_common.json" ||
        name == "combo.json" || name == "combinations.json")
    {
        return true;
    }
    return false;
}

String getComboFileType(const String &fullPath)
{
    if (fullPath == "/combo_common.json")
    {
        return "common";
    }
    if (fullPath.startsWith("/combo_"))
    {
        return "combo";
    }
    if (fullPath.startsWith("/my_combo_"))
    {
        return "custom";
    }
    if (fullPath == "/combo.json" || fullPath == "/combinations.json")
    {
        return "legacy";
    }
    return "unknown";
}

configWebServer::configWebServer() : server(80) {}

void configWebServer::begin()
{
    if (!FileSystemManager::ensureMounted())
    {
        Logger::getInstance().log("⚠️ Failed to mount LittleFS.");
        return;
    }
    Logger::getInstance().setWebServerActive(true);
    setupRoutes();
    server.addHandler(&events);
    server.begin();
    Logger::getInstance().log("✅ Web server started on port 80.");
}

void configWebServer::end()
{
    Logger::getInstance().setWebServerActive(false);
    server.end();
}

void configWebServer::updateStatus(const String &apIP, const String &staIP, const String &status)
{
    apIPAddress = apIP;
    staIPAddress = staIP;
    wifiStatus = status;
    Logger::getInstance().log("✅ Status updated: AP IP = " + apIP + ", STA IP = " + status);
}

void configWebServer::setupRoutes()
{

    server.serveStatic("/ui", LittleFS, "/ui");

    // --- Endpoint per la configurazione completa (es. WiFi) ---
    server.on("/config.json", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "application/json", readConfigFile()); });

    server.on("/config.json", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
        String newBody = "";
        for (size_t i = 0; i < len; i++) {
            newBody += (char)data[i];
        }
        Logger::getInstance().log("📥 Received partial update:");
        Logger::getInstance().log(newBody);

        // Legge il config corrente
        String currentConfigStr = readConfigFile();
        StaticJsonDocument<4096> currentConfig;
        DeserializationError error = deserializeJson(currentConfig, currentConfigStr);
        if (error) {
            Logger::getInstance().log("⚠️ Failed to parse current config: " + String(error.c_str()));
            request->send(500, "text/plain", "❌ Failed to parse current config.");
            return;
        }
        // Parsea il nuovo JSON in ingresso
        StaticJsonDocument<4096> newConfig;
        error = deserializeJson(newConfig, newBody);
        if (error) {
            Logger::getInstance().log("⚠️ Failed to parse new config: " + String(error.c_str()));
            request->send(400, "text/plain", "❌ Invalid JSON format.");
            return;
        }
        // Merge: per ogni chiave nuova, se esiste già viene aggiornato.
        JsonObject newConfigObj = newConfig.as<JsonObject>();
        for (JsonPair newPair : newConfigObj) {
            const char* key = newPair.key().c_str();
            JsonVariant newValue = newPair.value();
            if (currentConfig.containsKey(key)) {
                JsonVariant currentValue = currentConfig[key];
                if (newValue.is<JsonObject>() && currentValue.is<JsonObject>()) {
                    JsonObject newObj = newValue.as<JsonObject>();
                    JsonObject currentObj = currentValue.as<JsonObject>();
                    for (JsonPair innerPair : newObj) {
                        currentObj[innerPair.key()] = innerPair.value();
                    }
                } else {
                    currentConfig[key] = newValue;
                }
            } else {
                currentConfig[key] = newValue;
            }
        }
        // Salva la configurazione aggiornata
        String updatedJson;
        serializeJson(currentConfig, updatedJson);
        if (writeConfigFile(updatedJson)) {
            Logger::getInstance().log("💾 Saved Configuration:");
            Logger::getInstance().log(updatedJson);
            request->send(200, "text/plain", "✅ Configuration updated successfully! Restarting...");
            vTaskDelay(pdMS_TO_TICKS(1000)); // Dai tempo al tempo


            ESP.restart();
        } else {
            request->send(500, "text/plain", "❌ Failed to save configuration.");
        } });

    server.on("/scheduler/state", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                  request->send(200, "application/json", eventScheduler.buildStatusJson());
              });

    server.on("/scheduler/run", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr,
              [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
                  static String payload;
                  if (index == 0)
                  {
                      payload = "";
                      payload.reserve(total);
                  }
                  for (size_t i = 0; i < len; ++i)
                  {
                      payload += static_cast<char>(data[i]);
                  }
                  if (index + len < total)
                  {
                      return;
                  }

                  StaticJsonDocument<256> doc;
                  DeserializationError err = deserializeJson(doc, payload);
                  payload = "";
                  if (err)
                  {
                      request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                      return;
                  }

                  String id = doc["id"] | "";
                  String reason = doc["reason"] | "manual";
                  if (id.isEmpty())
                  {
                      request->send(400, "application/json", "{\"error\":\"Missing id\"}");
                      return;
                  }

                  if (eventScheduler.triggerEventById(id, reason.c_str()))
                  {
                      request->send(200, "application/json", "{\"status\":\"ok\"}");
                  }
                  else
                  {
                      request->send(404, "application/json", "{\"error\":\"Event not found or disabled\"}");
                  }
              });

    server.on("/scheduler/time", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr,
              [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
                  static String payload;
                  if (index == 0)
                  {
                      payload = "";
                      payload.reserve(total);
                  }
                  for (size_t i = 0; i < len; ++i)
                  {
                      payload += static_cast<char>(data[i]);
                  }
                  if (index + len < total)
                  {
                      return;
                  }

                  StaticJsonDocument<256> doc;
                  DeserializationError err = deserializeJson(doc, payload);
                  payload = "";
                  if (err)
                  {
                      request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                      return;
                  }

                  time_t epoch = doc["epoch"] | 0;
                  int tzMinutes = doc["timezone_minutes"] | 0;
                  if (epoch <= 0)
                  {
                      request->send(400, "application/json", "{\"error\":\"Invalid epoch\"}");
                      return;
                  }

                  if (eventScheduler.setManualTime(epoch, tzMinutes))
                  {
                      request->send(200, "application/json", "{\"status\":\"ok\"}");
                  }
                  else
                  {
                      request->send(500, "application/json", "{\"error\":\"Failed to set time\"}");
                  }
              });

    // --- Endpoint per la gestione dei dati IR ---
    server.on("/ir_data.json", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "application/json", readIrDataFile()); });

    server.on("/ir_data.json", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
        static String payload;
        if (index == 0)
        {
            payload = "";
        }

        payload.reserve(payload.length() + len);
        for (size_t i = 0; i < len; ++i)
        {
            payload += static_cast<char>(data[i]);
        }

        if (index + len == total)
        {
            Logger::getInstance().log("📥 Received IR data update, size: " + String(total));
            DynamicJsonDocument doc(32768);
            DeserializationError error = deserializeJson(doc, payload);
            if (error)
            {
                Logger::getInstance().log("⚠️ Failed to parse ir_data.json payload: " + String(error.c_str()));
                payload = "";
                request->send(400, "text/plain", "❌ Invalid JSON payload.");
                return;
            }

            if (!doc.containsKey("devices") || !doc["devices"].is<JsonObject>())
            {
                payload = "";
                request->send(400, "text/plain", "❌ JSON must contain a 'devices' object.");
                return;
            }

            if (doc.overflowed())
            {
                Logger::getInstance().log("⚠️ ir_data.json payload overflowed buffer.");
                payload = "";
                request->send(413, "text/plain", "❌ IR data payload too large.");
                return;
            }

            String normalized;
            serializeJson(doc, normalized);
            if (!writeIrDataFile(normalized))
            {
                payload = "";
                request->send(500, "text/plain", "❌ Failed to save ir_data.json.");
                return;
            }

            IRStorage *storage = inputHub.getIrStorage();
            if (storage)
            {
                if (storage->loadIRData())
                {
                    Logger::getInstance().log("🔄 IR storage reloaded after web update.");
                }
                else
                {
                    Logger::getInstance().log("⚠️ Failed to reload IR storage after web update.");
                }
            }
            else
            {
                Logger::getInstance().log("⚠️ IR storage not available to reload after web update.");
            }

            payload = "";
            request->send(200, "text/plain", "✅ IR data salvati con successo.");
        } });

    // --- Endpoint per la gestione delle "combinations" (originale) ---
    // GET: restituisce solo la sezione "combinations" del config.json
    server.on("/combinations.json", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        String configStr = readConfigFile();
        StaticJsonDocument<4096> configDoc;
        DeserializationError error = deserializeJson(configDoc, configStr);
        if (error) {
            Logger::getInstance().log("❌ Failed to parse config.json.");
            request->send(500, "text/plain", "❌ Failed to parse config.json.");
            return;
        }
        JsonVariant combinations = configDoc["combinations"];
        String response;
        if (combinations.isNull()) {
            response = "{}";
        } else {
            serializeJson(combinations, response);
        }
        request->send(200, "application/json", response); });

    // POST: aggiorna la sezione "combinations" nel config.json
    server.on("/combinations.json", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
        String newBody = "";
        for (size_t i = 0; i < len; i++) {
            newBody += (char)data[i];
        }
        Logger::getInstance().log("📥 Received combinations update:");
        Logger::getInstance().log(newBody);

        String currentConfigStr = readConfigFile();
        StaticJsonDocument<4096> configDoc;
        DeserializationError error = deserializeJson(configDoc, currentConfigStr);
        if (error) {
            Logger::getInstance().log("⚠️ Failed to parse current config: " + String(error.c_str()));
            request->send(500, "text/plain", "❌ Failed to parse current config.");
            return;
        }
        StaticJsonDocument<4096> newCombDoc;
        error = deserializeJson(newCombDoc, newBody);
        if (error) {
            Logger::getInstance().log("⚠️ Failed to parse new combinations: " + String(error.c_str()));
            request->send(400, "text/plain", "❌ Invalid JSON format for combinations.");
            return;
        }
        configDoc["combinations"] = newCombDoc.as<JsonVariant>();
        String updatedJson;
        serializeJson(configDoc, updatedJson);
        if (writeConfigFile(updatedJson)) {
            Logger::getInstance().log("💾 Saved Combinations Configuration:");
            Logger::getInstance().log(updatedJson);
            request->send(200, "text/plain", "✅ Combinations updated successfully! Restarting...");
             vTaskDelay(pdMS_TO_TICKS(1000)); // Dai tempo al tempo
            ESP.restart();
        } else {
            request->send(500, "text/plain", "❌ Failed to save combinations configuration.");
        } });

    // --- Endpoint per il nuovo file combo.json ---
    // GET: ottiene l'intero file combo.json
    server.on("/combo.json", HTTP_GET, [](AsyncWebServerRequest *request)
              { 
                String comboJson = readComboFile();
                request->send(200, "application/json", comboJson); });

    // POST: aggiorna il file combo.json completo
    server.on("/combo.json", HTTP_POST, [](AsyncWebServerRequest *request)
              {
                  // Funzione di callback vuota richiesta
              },
              [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
              {
                  // Gestione upload - non usata qui
              },
              [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
                static String jsonBuffer = "";
                
                // Aggiungiamo i dati ricevuti al buffer
                for (size_t i = 0; i < len; i++) {
                    jsonBuffer += (char)data[i];
                }
                
                // Se abbiamo ricevuto tutti i dati, processiamo il JSON
                if (index + len == total) {
                    Logger::getInstance().log("📥 Received combo update, total size: " + String(total));
                    
                    // Parsifichiamo il JSON solo per verificare che sia valido
                    DynamicJsonDocument doc(16384); // Aumentiamo la dimensione del buffer
                    DeserializationError error = deserializeJson(doc, jsonBuffer);
                    
                    if (error) {
                        Logger::getInstance().log("⚠️ Failed to parse combo.json: " + String(error.c_str()));
                        request->send(400, "text/plain", "❌ Invalid JSON format");
                    } else {
                        // Se è valido, lo salviamo direttamente
                        if (writeComboFile(jsonBuffer)) {
                            Logger::getInstance().log("💾 Saved combo.json successfully");
                            request->send(200, "text/plain", "✅ Combinations updated successfully! Restarting...");
                            // Ritardiamo il riavvio per assicurarci che la risposta venga inviata
                             vTaskDelay(pdMS_TO_TICKS(1000)); // Dai tempo al tempo
                            ESP.restart();
                        } else {
                            request->send(500, "text/plain", "❌ Failed to save combo.json");
                        }
                    }
                    
                    // Reset del buffer per la prossima richiesta
                    jsonBuffer = "";
                } });

    // --- Endpoint per ottenere un set specifico di combinazioni ---
    server.on("/combo_set", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                if (!request->hasArg("set")) {
                    request->send(400, "text/plain", "Missing 'set' parameter");
                    return;
                }
                
                int setNumber = request->arg("set").toInt();
                String setKey = "combinations_" + String(setNumber);
                
                String comboStr = readComboFile();
                DynamicJsonDocument doc(16384);
                DeserializationError error = deserializeJson(doc, comboStr);
                
                if (error) {
                    Logger::getInstance().log("❌ Failed to parse combo.json: " + String(error.c_str()));
                    request->send(500, "text/plain", "❌ Failed to parse combo.json");
                    return;
                }
                
                JsonVariant setData = doc[setKey];
                String response;
                if (setData.isNull()) {
                    response = "{}";
                } else {
                    serializeJson(setData, response);
                }
                
                request->send(200, "application/json", response); });

    // --- Endpoint per aggiornare un set specifico di combinazioni ---
    server.on("/combo_set", HTTP_POST, [](AsyncWebServerRequest *request)
              {
                  // Funzione di callback vuota richiesta
              },
              [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
              {
                  // Gestione upload - non usata qui
              },
              [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
                static String jsonBuffer = "";
                
                if (!request->hasArg("set")) {
                    request->send(400, "text/plain", "Missing 'set' parameter");
                    return;
                }
                
                int setNumber = request->arg("set").toInt();
                String setKey = "combinations_" + String(setNumber);
                
                // Aggiungiamo i dati ricevuti al buffer
                for (size_t i = 0; i < len; i++) {
                    jsonBuffer += (char)data[i];
                }
                
                // Se abbiamo ricevuto tutti i dati, processiamo il JSON
                if (index + len == total) {
                    Logger::getInstance().log("📥 Received update for " + setKey + ", size: " + String(total));
                    
                    // Leggiamo l'intero file combo.json
                    String comboStr = readComboFile();
                    DynamicJsonDocument comboDoc(16384);
                    DeserializationError error = deserializeJson(comboDoc, comboStr);
                    
                    if (error) {
                        Logger::getInstance().log("⚠️ Failed to parse current combo.json: " + String(error.c_str()));
                        request->send(500, "text/plain", "❌ Failed to parse current combo.json");
                        jsonBuffer = "";
                        return;
                    }
                    
                    // Parsifichiamo il nuovo set di combinazioni
                    DynamicJsonDocument newSetDoc(8192);
                    error = deserializeJson(newSetDoc, jsonBuffer);
                    
                    if (error) {
                        Logger::getInstance().log("⚠️ Failed to parse new set: " + String(error.c_str()));
                        request->send(400, "text/plain", "❌ Invalid JSON format for set");
                        jsonBuffer = "";
                        return;
                    }
                    
                    // Aggiorniamo il set specifico
                    comboDoc[setKey] = newSetDoc.as<JsonVariant>();
                    
                    // Serializziamo l'intero documento
                    String updatedJson;
                    serializeJson(comboDoc, updatedJson);
                    
                    // Salviamo il file aggiornato
                    if (writeComboFile(updatedJson)) {
                        Logger::getInstance().log("💾 Saved combo.json with updated " + setKey);
                        request->send(200, "text/plain", "✅ Combination set updated successfully! Restarting...");
                         vTaskDelay(pdMS_TO_TICKS(1000)); // Dai tempo al tempo
                        ESP.restart();
                    } else {
                        request->send(500, "text/plain", "❌ Failed to save combo.json");
                    }
                    
                    // Reset del buffer per la prossima richiesta
                    jsonBuffer = "";
                } });

    // --- Endpoint per elencare e salvare direttamente i file combo ---
    server.on("/combo_files.json", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        DynamicJsonDocument doc(16384);  // Reduced buffer size since we're not sending content
        JsonArray files = doc.createNestedArray("files");

        File rootDir = LittleFS.open("/");
        if (!rootDir)
        {
            Logger::getInstance().log("⚠️ Failed to open LittleFS root while listing combo files.");
            request->send(500, "text/plain", "❌ Unable to list combo files.");
            return;
        }

        std::vector<String> fileNames;
        for (File entry = rootDir.openNextFile(); entry; entry = rootDir.openNextFile())
        {
            if (entry.isDirectory())
            {
                entry.close();
                continue;
            }

            String name = entry.name();
            // Normalize: ensure name starts with /
            if (!name.startsWith("/"))
            {
                name = "/" + name;
            }

            if (name == "/combo_common.json" || name.startsWith("/combo_") || name.startsWith("/my_combo_") ||
                name == "/combo.json" || name == "/combinations.json")
            {
                Logger::getInstance().log("✓ Found combo file: " + name);
                fileNames.push_back(name);
            }
            entry.close();
        }
        rootDir.close();

        Logger::getInstance().log("📂 Found " + String(fileNames.size()) + " combo files in total");

        std::sort(fileNames.begin(), fileNames.end(), [](const String &a, const String &b)
                  { return a < b; });

        for (const String &fullName : fileNames)
        {
            File comboFile = LittleFS.open(fullName, "r");
            if (!comboFile)
            {
                Logger::getInstance().log("⚠️ Failed to open " + fullName + " while listing combo files.");
                continue;
            }

            String rawContent = comboFile.readString();
            comboFile.close();

            JsonObject fileObj = files.createNestedObject();
            String fileName = fullName.substring(1); // remove leading slash
            fileObj["name"] = fileName;
            fileObj["type"] = getComboFileType(fullName);
            fileObj["content"] = rawContent;  // We still include content for now - will load on demand later
            Logger::getInstance().log("  → Added: " + fileName + " (" + String(rawContent.length()) + " bytes)");
        }

        if (doc.overflowed())
        {
            Logger::getInstance().log("⚠️ JSON document overflowed while building combo file list");
            request->send(500, "text/plain", "❌ Internal error: combo list too large.");
            return;
        }

        size_t jsonSize = measureJson(doc);
        Logger::getInstance().log("📊 JSON size: " + String(jsonSize) + " bytes");

        String payload;
        if (!payload.reserve(jsonSize + 1))
        {
            Logger::getInstance().log("⚠️ Failed to reserve payload buffer of " + String(jsonSize + 1) + " bytes");
            request->send(500, "text/plain", "❌ Out of memory while preparing response.");
            return;
        }

        serializeJson(doc, payload);
        Logger::getInstance().log("📤 Sending combo_files.json payload, size: " + String(payload.length()) + " bytes");
        request->send(200, "application/json", payload); });

    server.on("/combo_files.json", HTTP_POST, [](AsyncWebServerRequest *request) {},
              nullptr,
              [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
        static String comboFilePayload;

        if (index == 0)
        {
            comboFilePayload = "";
        }

        comboFilePayload.reserve(comboFilePayload.length() + len);
        for (size_t i = 0; i < len; ++i)
        {
            comboFilePayload += static_cast<char>(data[i]);
        }

        if (index + len == total)
        {
            Logger::getInstance().log("📥 Received complete payload, size: " + String(total) + " bytes");
            Logger::getInstance().log("📥 Payload preview: " + comboFilePayload.substring(0, 200));

            DynamicJsonDocument doc(8192);  // Reduced from 65536
            DeserializationError error = deserializeJson(doc, comboFilePayload);

            if (error)
            {
                Logger::getInstance().log("⚠️ Failed to parse combo_files payload: " + String(error.c_str()));
                Logger::getInstance().log("⚠️ Payload was: " + comboFilePayload);
                comboFilePayload = "";
                request->send(400, "text/plain", "❌ Invalid JSON payload.");
                return;
            }

            comboFilePayload = "";

            String name = doc["name"] | "";
            Logger::getInstance().log("📝 Attempting to save file: " + name);

            if (!isValidComboFilename(name))
            {
                Logger::getInstance().log("⚠️ Invalid combo file name: " + name);
                request->send(400, "text/plain", "❌ Invalid combo file name.");
                return;
            }

            if (!doc.containsKey("content"))
            {
                request->send(400, "text/plain", "❌ Missing content field.");
                return;
            }

            JsonVariant contentVariant = doc["content"];
            String contentStr;

            Logger::getInstance().log("📦 Content type check...");

            if (contentVariant.is<const char *>() || contentVariant.is<String>())
            {
                contentStr = contentVariant.as<String>();
                Logger::getInstance().log("✓ Content is string, length: " + String(contentStr.length()));
            }
            else if (contentVariant.is<JsonObject>() || contentVariant.is<JsonArray>())
            {
                serializeJson(contentVariant, contentStr);
                Logger::getInstance().log("✓ Content is JSON, serialized length: " + String(contentStr.length()));
            }
            else
            {
                Logger::getInstance().log("❌ Unsupported content format.");
                request->send(400, "text/plain", "❌ Unsupported content format.");
                return;
            }

            String path = "/" + name;
            Logger::getInstance().log("💾 Opening file for writing: " + path);

            File file = LittleFS.open(path.c_str(), "w");
            if (!file)
            {
                Logger::getInstance().log("⚠️ Failed to open " + path + " for writing.");
                request->send(500, "text/plain", "❌ Failed to save combo file.");
                return;
            }

            size_t written = file.print(contentStr);
            file.close();

            Logger::getInstance().log("💾 Saved combo file " + path + " (" + String(written) + " bytes written)");
            request->send(200, "text/plain", "✅ File combo salvato con successo! Riavvio...");
            vTaskDelay(pdMS_TO_TICKS(1000)); // Dai tempo al tempo
            ESP.restart();
        }
              });

    // --- Endpoint per la gestione delle "advanced" (tutte le altre config) ---
    // GET: restituisce tutte le sezioni del config.json tranne "wifi" e "combinations"
    server.on("/advanced.json", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        String configStr = readConfigFile();
        StaticJsonDocument<4096> configDoc;
        DeserializationError error = deserializeJson(configDoc, configStr);
        if (error) {
            Logger::getInstance().log("❌ Failed to parse config.json.");
            request->send(500, "text/plain", "❌ Failed to parse config.json.");
            return;
        }
        StaticJsonDocument<4096> advancedDoc;
        JsonObject advanced = advancedDoc.to<JsonObject>();
        JsonObject configObj = configDoc.as<JsonObject>();
        // Copia tutte le chiavi tranne "wifi" e "combinations"
        for (JsonPair kv : configObj) {
            const char* key = kv.key().c_str();
            if ((strcmp(key, "wifi") != 0) && (strcmp(key, "combinations") != 0)) {
                advanced[key] = kv.value();
            }
        }
        String response;
        serializeJson(advanced, response);
        request->send(200, "application/json", response); });

    // POST: aggiorna le sezioni avanzate nel config.json (tranne "wifi" e "combinations")
    server.on("/advanced.json", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
        String newBody = "";
        for (size_t i = 0; i < len; i++) {
            newBody += (char)data[i];
        }
        Logger::getInstance().log("📥 Received advanced config update:");
        Logger::getInstance().log(newBody);

        String currentConfigStr = readConfigFile();
        StaticJsonDocument<4096> configDoc;
        DeserializationError error = deserializeJson(configDoc, currentConfigStr);
        if (error) {
            Logger::getInstance().log("⚠️ Failed to parse current config: " + String(error.c_str()));
            request->send(500, "text/plain", "❌ Failed to parse current config.");
            return;
        }
        StaticJsonDocument<4096> newAdvDoc;
        error = deserializeJson(newAdvDoc, newBody);
        if (error) {
            Logger::getInstance().log("⚠️ Failed to parse new advanced config: " + String(error.c_str()));
            request->send(400, "text/plain", "❌ Invalid JSON format for advanced config.");
            return;
        }
        JsonObject newAdvObj = newAdvDoc.as<JsonObject>();
        // Aggiorna solo le chiavi che non sono "wifi" e "combinations"
        for (JsonPair advPair : newAdvObj) {
            const char* key = advPair.key().c_str();
            if ((strcmp(key, "wifi") != 0) && (strcmp(key, "combinations") != 0)) {
                configDoc[key] = advPair.value();
            }
        }
        String updatedJson;
        serializeJson(configDoc, updatedJson);
        if (writeConfigFile(updatedJson)) {
            Logger::getInstance().log("💾 Saved Advanced Configuration:");
            Logger::getInstance().log(updatedJson);
            request->send(200, "text/plain", "✅ Advanced config updated successfully! Restarting...");
             vTaskDelay(pdMS_TO_TICKS(1000)); // Dai tempo al tempo
            ESP.restart();
        } else {
            request->send(500, "text/plain", "❌ Failed to save advanced configuration.");
        } });

    // --- Endpoint per elencare le special actions disponibili ---
    server.on("/special_actions.json", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        StaticJsonDocument<2048> doc;
        JsonArray actions = doc.createNestedArray("actions");
        for (const auto &action : kSpecialActions)
        {
            JsonObject item = actions.createNestedObject();
            item["id"] = action.id;
            item["label"] = action.label;
            item["endpoint"] = action.endpoint;
            item["method"] = action.method;
            item["description"] = action.description;
            item["requiresParams"] = action.requiresParams;
            if (action.examplePayload)
            {
                item["example"] = action.examplePayload;
            }
            if (action.actionId)
            {
                item["actionId"] = action.actionId;
            }
        }
        String payload;
        serializeJson(doc, payload);
        request->send(200, "application/json", payload); });

    // --- Endpoint unificato per richiamare le special actions parametrizzabili ---
    server.on("/special_action", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
        static String payload;
        if (index == 0)
        {
            payload = "";
        }

        payload.reserve(payload.length() + len);
        for (size_t i = 0; i < len; ++i)
        {
            payload += static_cast<char>(data[i]);
        }

        if (index + len == total)
        {
            DynamicJsonDocument doc(2048);
            DeserializationError error = deserializeJson(doc, payload);
            if (error)
            {
                Logger::getInstance().log("⚠️ Failed to parse special_action payload: " + String(error.c_str()));
                payload = "";
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Payload JSON non valido\"}");
                return;
            }

            String actionId = doc["actionId"] | "";
            JsonVariantConst params = doc["params"];

            if (actionId.isEmpty())
            {
                payload = "";
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Campo actionId obbligatorio\"}");
                return;
            }

            String message;
            int statusCode = 200;
            bool handled = handleSpecialActionRequest(actionId, params, message, statusCode);
            payload = "";

            StaticJsonDocument<256> response;
            response["action"] = actionId;
            response["message"] = message;
            response["status"] = (statusCode >= 200 && statusCode < 300 && handled) ? "ok" : "error";
            String responseBody;
            serializeJson(response, responseBody);
            request->send(statusCode, "application/json", responseBody);
        } });

    // --- Endpoint per servire la pagina principale (config.html) ---
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (!LittleFS.exists("/config.html"))
        {
            Logger::getInstance().log("❌ config.html not found");
            request->send(404, "text/plain", "❌ config.html not found");
            return;
        }
        request->send(LittleFS, "/config.html", "text/html"); });

    server.on("/status.json", HTTP_GET, [this](AsyncWebServerRequest *request)
              {
        StaticJsonDocument<256> doc;
        doc["wifi_status"] = wifiStatus;
        doc["ap_ip"] = apIPAddress;
        doc["sta_ip"] = staIPAddress;
        String payload;
        serializeJson(doc, payload);
        request->send(200, "application/json", payload); });

    // --- Endpoint per servire la pagina delle combinations (combinations.html) ---
    server.on("/combinations.html", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        File file = LittleFS.open("/combinations.html", "r");
        if (!file) {
            Logger::getInstance().log("❌ combinations.html not found");
            request->send(404, "text/plain", "❌ combinations.html not found");
            return;
        }
        String html = file.readString();
        file.close();
        request->send(200, "text/html", html); });

    // --- Endpoint per servire la pagina delle combo aggiornata (combo.html) ---
    server.on("/combo.html", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        File file = LittleFS.open("/combo.html", "r");
        if (!file) {
            Logger::getInstance().log("❌ combo.html not found");
            request->send(404, "text/plain", "❌ combo.html not found");
            return;
        }
        String html = file.readString();
        file.close();
        request->send(200, "text/html", html); });

    // --- Endpoint per servire la pagina Advanced (advanced.html) ---
    server.on("/advanced.html", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        File file = LittleFS.open("/advanced.html", "r");
        if (!file) {
            Logger::getInstance().log("❌ advanced.html not found");
            request->send(404, "text/plain", "❌ advanced.html not found");
            return;
        }
        String html = file.readString();
        file.close();
        request->send(200, "text/html", html); });

    // --- Endpoint per servire la pagina Special Actions (special_actions.html) ---
    server.on("/special_actions.html", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (!LittleFS.exists("/special_actions.html")) {
            Logger::getInstance().log("❌ special_actions.html not found");
            request->send(404, "text/plain", "❌ special_actions.html not found");
            return;
        }
        // Stream the file directly to avoid large heap allocations on bigger pages.
        request->send(LittleFS, "/special_actions.html", "text/html"); });

    server.on("/scheduler.html", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (!LittleFS.exists("/scheduler.html")) {
            Logger::getInstance().log("❌ scheduler.html not found");
            request->send(404, "text/plain", "❌ scheduler.html not found");
            return;
        }
        request->send(LittleFS, "/scheduler.html", "text/html");
              });

    // --- Special Actions Endpoints ---
    server.on("/resetDevice", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        Logger::getInstance().log("🔄 Resetting device...");
        specialAction.resetDevice();
        request->send(200, "text/plain", "✅ Device is resetting..."); });

    server.on("/calibrateSensor", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        Logger::getInstance().log(" calibrating Sensor...");
        specialAction.calibrateSensor();
        request->send(200, "text/plain", "✅ Calibrating Sensor..."); });

    server.on("/executeGesture", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        String pressedParam = request->arg("pressed");
        bool pressed = (pressedParam == "true");
        Logger::getInstance().log(" executing Gesture...");

        specialAction.executeGesture(pressed);
        request->send(200, "text/plain", "✅ executing Gesture..."); });

    server.on("/printMemoryInfo", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        Logger::getInstance().log(" printing Memory Info...");
        specialAction.printMemoryInfo();
        request->send(200, "text/plain", "✅ printing Memory Info..."); });

    // Aggiungi l'output per gli eventi solo se non è già stato aggiunto.
    if (!eventsOutputAdded)
    {
        Logger::getInstance().addOutput([&](const String &msg)
                                        { events.send(msg.c_str(), "message", millis()); });
        eventsOutputAdded = true;
    }
    else
    {
        Logger::getInstance().log("✅ Reuse logger output");
    }

    Logger::getInstance().log("✅ Routes set up successfully.");
}
