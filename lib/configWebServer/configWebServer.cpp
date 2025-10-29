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
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <algorithm>
#include <cctype>
#include <vector>
#include "Logger.h"

AsyncWebServer server(80);
AsyncEventSource events("/log");

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

// Legge il file gesture_features.json
String readGestureFeatureFile()
{
    File file = LittleFS.open("/gesture_features.json", "r");
    if (!file)
    {
        Logger::getInstance().log("⚠️ Failed to open gesture_features.json");
        return "{}"; // Ritorna un JSON vuoto
    }
    String json = file.readString();
    file.close();
    if (json.length() == 0)
    {
        Logger::getInstance().log("⚠️ gesture_features.json is empty!");
        return "{}";
    }
    return json;
}

// Scrive il file gesture_features.json
bool writeGestureFeatureFile(const String &json)
{
    File file = LittleFS.open("/gesture_features.json", "w");
    if (!file)
    {
        Logger::getInstance().log("⚠️ Failed to open gesture_features.json for writing.");
        return false;
    }
    file.print(json);
    file.close();
    return true;
}

configWebServer::configWebServer() : server(80) {}

void configWebServer::begin()
{
    if (!LittleFS.begin())
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

        String payload;
        size_t jsonSize = measureJson(doc);
        Logger::getInstance().log("📊 JSON size: " + String(jsonSize) + " bytes");

        serializeJson(doc, payload);
        Logger::getInstance().log("📤 Sending combo_files.json payload, size: " + String(payload.length()) + " bytes");

        // Send with chunked transfer encoding if too large
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        serializeJson(doc, *response);
        request->send(response); });

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
        File file = LittleFS.open("/special_actions.html", "r");
        if (!file) {
            Logger::getInstance().log("❌ special_actions.html not found");
            request->send(404, "text/plain", "❌ special_actions.html not found");
            return;
        }
        String html = file.readString();
        file.close();
        request->send(200, "text/html", html); });

    // Endpoint per gestire gesture_features.json
    // GET: restituisce il contenuto del file gesture_features.json
    server.on("/gesture_features.json", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "application/json", readGestureFeatureFile()); });

    // POST: aggiorna il file gesture_features.json con il nuovo contenuto JSON
    server.on("/gesture_features.json", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
        String newBody = "";
        for (size_t i = 0; i < len; i++) {
            newBody += (char)data[i];
        }
        Logger::getInstance().log("📥 Received gesture_features update:");
        Logger::getInstance().log(newBody);

        if (writeGestureFeatureFile(newBody)) {
            Logger::getInstance().log("💾 Saved gesture_features configuration:");
            Logger::getInstance().log(newBody);
            request->send(200, "text/plain", "✅ Gesture feature configuration updated successfully! Restarting...");
             vTaskDelay(pdMS_TO_TICKS(1000)); // Dai tempo al tempo
            ESP.restart();
        } else {
            request->send(500, "text/plain", "❌ Failed to save gesture feature configuration.");
        } });

    // Endpoint per servire la pagina HTML per gesture_features.json
    server.on("/gesture_features.html", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        File file = LittleFS.open("/gesture_features.html", "r");
        if (!file) {
            request->send(404, "text/plain", "❌ gesture_features.html not found");
            return;
        }
        String html = file.readString();
        file.close();
        request->send(200, "text/html", html); });

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

    server.on("/toggleSampling", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        String pressedParam = request->arg("pressed");
        bool pressed = (pressedParam == "true");
        Logger::getInstance().log(" toggling Sampling...");
        specialAction.toggleSampling(pressed);
        request->send(200, "text/plain", "✅ Toggling Sampling..."); });

    server.on("/clearAllGestures", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        Logger::getInstance().log(" clearing All Gestures...");
        specialAction.clearAllGestures();
        request->send(200, "text/plain", "✅ Clearing All Gestures..."); });

    // Endpoint aggiornato per clearGestureWithID
    server.on("/clearGestureWithID", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        if (request->hasArg("key")) {
            int key = request->arg("key").toInt();
            Logger::getInstance().log(" clearing Gesture With ID " + String(key) + "...");
            specialAction.clearGestureWithID(key);
            request->send(200, "text/plain", "✅ Clearing Gesture With ID " + String(key) + "...");
        } else {
            Logger::getInstance().log("⚠️ Missing 'key' parameter for clearGestureWithID");
            request->send(400, "text/plain", "❌ Missing key parameter.");
        } });

    server.on("/convertJsonToBinary", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        Logger::getInstance().log(" converting Json To Binary...");
        specialAction.convertJsonToBinary();
        request->send(200, "text/plain", "✅ converting Json To Binary..."); });

    server.on("/printJson", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        Logger::getInstance().log(" printing Json...");
        specialAction.printJson();
        request->send(200, "text/plain", "✅ printing Json..."); });

    server.on("/trainGesture", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        String pressedParam = request->arg("pressed");
        bool pressed = (pressedParam == "true");
        
        if (!pressed) { // When training stops, a key is required.
            if (!request->hasArg("key")) {
                Logger::getInstance().log("⚠️ Missing 'key' parameter for trainGesture when pressed is false");
                request->send(400, "text/plain", "❌ Missing key parameter when stopping training.");
                return;
            }
            int key = request->arg("key").toInt();
            Logger::getInstance().log(" training Gesture stop with key " + String(key) + "...");
            // Call the version that accepts a key parameter. Make sure to implement this.
            specialAction.trainGesture(pressed, key);
            request->send(200, "text/plain", "✅ Training gesture stopped with key " + String(key) + "...");
        } else {
            Logger::getInstance().log(" training Gesture start...");
            specialAction.trainGesture(pressed);
            request->send(200, "text/plain", "✅ Training gesture started...");
        } });

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
