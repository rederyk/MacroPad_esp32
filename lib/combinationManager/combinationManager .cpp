// combinationManager.cpp
#include "combinationManager.h"
#include <LittleFS.h>
#include "Logger.h"

CombinationManager::CombinationManager() {}

bool CombinationManager::loadCombinations() {
    if (!LittleFS.begin(true)) {
        Logger::getInstance().log("Failed to mount LittleFS");
        return false;
    }

    File comboFile = LittleFS.open("/combo.json");
    if (!comboFile) {
        Logger::getInstance().log("Failed to open combo file");
        LittleFS.end();
        return false;
    }

    DeserializationError error = deserializeJson(doc, comboFile);
    if (error) {
        Logger::getInstance().log("Failed to parse combo file: " + String(error.c_str()));
        comboFile.close();
        LittleFS.end();
        return false;
    }

    comboFile.close();
    LittleFS.end();

    // Store combinations
    if (doc.containsKey("combinations")) {
        combinations = doc["combinations"];
        
        // Log loaded combinations
        Logger::getInstance().log("Loaded combinations:");
        for (JsonPair combo : combinations) {
            String logMessage = "  " + String(combo.key().c_str()) + ": ";
            
            for (JsonVariant action : combo.value().as<JsonArray>()) {
                logMessage += String(action.as<const char*>()) + " ";
            }
            
            Logger::getInstance().log(logMessage);
        }
        
        return true;
    } else {
        Logger::getInstance().log("No combinations found in combo file");
        return false;
    }
}

JsonObject CombinationManager::getCombinations() {
    return combinations;
}