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


#include "combinationManager.h"
#include <LittleFS.h>
#include "Logger.h"

CombinationManager::CombinationManager() {}

bool CombinationManager::loadJsonFile(const char* filepath, JsonObject& target)
{
    File file = LittleFS.open(filepath);
    if (!file)
    {
        Logger::getInstance().log("Failed to open file: " + String(filepath));
        return false;
    }

    // Usa un buffer dinamico per il caricamento temporaneo
    // I file combo_X.json sono massimo 1.5KB, ma ArduinoJson ha overhead
    DynamicJsonDocument tempDoc(2560);  // Aumentato per overhead JSON
    DeserializationError error = deserializeJson(tempDoc, file);
    file.close();

    if (error)
    {
        Logger::getInstance().log("Failed to parse " + String(filepath) + ": " + String(error.c_str()));
        return false;
    }

    // Copy all key-value pairs to target
    for (JsonPair kv : tempDoc.as<JsonObject>())
    {
        target[kv.key()] = kv.value();
    }

    return true;
}

bool CombinationManager::mergeJsonFile(const char* filepath, JsonObject& target)
{
    return loadJsonFile(filepath, target);
}

bool CombinationManager::loadCombinations(int setNumber)
{
    if (!LittleFS.begin(true))
    {
        Logger::getInstance().log("Failed to mount LittleFS");
        return false;
    }

    // Clear previous data
    doc.clear();
    combinations = doc.to<JsonObject>();

    // Load common combinations first
    if (!loadJsonFile("/combo_common.json", combinations))
    {
        Logger::getInstance().log("Warning: Failed to load common combinations");
        // Continue anyway, common file is optional
    }

    // Load device-specific combinations
    String comboFilePath = "/combo_" + String(setNumber) + ".json";

    if (!mergeJsonFile(comboFilePath.c_str(), combinations))
    {
        // Fallback to combo_0.json if requested set doesn't exist
        Logger::getInstance().log("Set " + String(setNumber) + " not found, falling back to set 0");

        if (!mergeJsonFile("/combo_0.json", combinations))
        {
            Logger::getInstance().log("Failed to load combo_0.json");
            LittleFS.end();
            return false;
        }
    }

    LittleFS.end();

    // Validate that we have at least some combinations
    if (combinations.size() == 0)
    {
        Logger::getInstance().log("No combinations loaded!");
        return false;
    }

    // Log loaded combinations
    Logger::getInstance().log("Loaded combination set " + String(setNumber) + " (" + String(combinations.size()) + " entries):");
    for (JsonPair combo : combinations)
    {
        String logMessage = "  " + String(combo.key().c_str()) + ": ";

        for (JsonVariant action : combo.value().as<JsonArray>())
        {
            logMessage += String(action.as<const char *>()) + " ";
        }

        Logger::getInstance().log(logMessage);
    }

    return true;
}

JsonObject CombinationManager::getCombinations()
{
    return combinations;
}
