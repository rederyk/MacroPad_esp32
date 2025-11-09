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
#include "FileSystemManager.h"

CombinationManager::CombinationManager() : currentSetNumber(0), currentPrefix("combo") {}

bool CombinationManager::loadJsonFile(const char* filepath, JsonObject& target)
{
    File file = LittleFS.open(filepath);
    if (!file)
    {
        Logger::getInstance().log("Failed to open file: " + String(filepath));
        return false;
    }

    // Check file size before loading
    size_t fileSize = file.size();
    Logger::getInstance().log("Loading " + String(filepath) + " (size: " + String(fileSize) + " bytes)");

    if (fileSize > COMBO_FILE_WARNING_SIZE)
    {
        Logger::getInstance().log("WARNING: File is large, may cause memory issues (limit: " +
                                String(COMBO_FILE_WARNING_SIZE) + " bytes)");
    }

    // Usa un buffer dinamico per il caricamento temporaneo
    // Size is configurable via COMBO_TEMP_DOC_SIZE
    DynamicJsonDocument tempDoc(COMBO_TEMP_DOC_SIZE);
    DeserializationError error = deserializeJson(tempDoc, file);
    file.close();

    if (error)
    {
        String errorMsg = "Failed to parse " + String(filepath) + ": " + String(error.c_str());

        // Specific error messages for common issues
        if (error == DeserializationError::NoMemory)
        {
            errorMsg += " - File too large! Max buffer: " + String(COMBO_TEMP_DOC_SIZE) +
                       " bytes (file is " + String(fileSize) + " bytes)";
        }
        else if (error == DeserializationError::InvalidInput)
        {
            errorMsg += " - Invalid JSON format, check syntax";
        }

        Logger::getInstance().log(errorMsg);
        return false;
    }

    // Copy all key-value pairs to target
    for (JsonPair kv : tempDoc.as<JsonObject>())
    {
        target[kv.key()] = kv.value();
    }

    Logger::getInstance().log("Successfully loaded " + String(filepath));
    return true;
}

bool CombinationManager::mergeJsonFile(const char* filepath, JsonObject& target)
{
    return loadJsonFile(filepath, target);
}

void CombinationManager::parseSettings(JsonObject& obj)
{
    // Reset settings to defaults
    settings = ComboSettings();

    // Look for _settings key
    if (obj.containsKey("_settings"))
    {
        JsonObject settingsObj = obj["_settings"].as<JsonObject>();

        // Parse LED color if present
        if (settingsObj.containsKey("led_color"))
        {
            JsonArray ledColor = settingsObj["led_color"].as<JsonArray>();
            if (ledColor.size() == 3)
            {
                settings.ledR = ledColor[0].as<int>();
                settings.ledG = ledColor[1].as<int>();
                settings.ledB = ledColor[2].as<int>();

                Logger::getInstance().log("  Loaded settings: LED color RGB(" +
                    String(settings.ledR) + "," +
                    String(settings.ledG) + "," +
                    String(settings.ledB) + ")");
            }
        }

        // Parse interactive lighting colors if present
        if (settingsObj.containsKey("interactive_colors"))
        {
            JsonArray interactiveColorsArray = settingsObj["interactive_colors"].as<JsonArray>();
            settings.interactiveColors.clear();

            for (JsonVariant colorVariant : interactiveColorsArray)
            {
                JsonArray colorArray = colorVariant.as<JsonArray>();
                if (colorArray.size() == 3)
                {
                    std::array<int, 3> rgb = {
                        colorArray[0].as<int>(),
                        colorArray[1].as<int>(),
                        colorArray[2].as<int>()
                    };
                    settings.interactiveColors.push_back(rgb);
                }
            }

            if (settings.hasInteractiveColors())
            {
                Logger::getInstance().log("  Loaded " + String(settings.interactiveColors.size()) +
                                        " interactive lighting colors");
            }
        }
    }
}

bool CombinationManager::loadCombinationsInternal(int setNumber, const char* prefix)
{
    if (!FileSystemManager::ensureMounted())
    {
        Logger::getInstance().log("Failed to mount LittleFS");
        return false;
    }

    // Log memory configuration
    Logger::getInstance().log("Combo memory config - Buffer: " + String(COMBO_MAIN_DOC_SIZE) +
                            " bytes, Temp: " + String(COMBO_TEMP_DOC_SIZE) + " bytes");

    // Clear previous data
    doc.clear();
    combinations = doc.to<JsonObject>();

    // Load common combinations first (always use combo_common.json)
    if (!loadJsonFile("/combo_common.json", combinations))
    {
        Logger::getInstance().log("Warning: Failed to load common combinations");
        // Continue anyway, common file is optional
    }

    // Load device-specific combinations with prefix
    String comboFilePath = "/" + String(prefix) + "_" + String(setNumber) + ".json";

    if (!mergeJsonFile(comboFilePath.c_str(), combinations))
    {
        // Fallback to combo_0.json if requested set doesn't exist
        Logger::getInstance().log("Set " + String(setNumber) + " not found with prefix '" + String(prefix) + "', falling back to combo_0");

        if (!mergeJsonFile("/combo_0.json", combinations))
        {
            Logger::getInstance().log("Failed to load combo_0.json");
            return false;
        }
        // Reset to default if fallback was used
        currentSetNumber = 0;
        currentPrefix = "combo";
    }
    else
    {
        // Successfully loaded requested set
        currentSetNumber = setNumber;
        currentPrefix = String(prefix);
    }

    // Parse settings while filesystem is available
    parseSettings(combinations);

    // Validate that we have at least some combinations (excluding _settings)
    int comboCount = combinations.size();
    if (combinations.containsKey("_settings"))
    {
        comboCount--;  // Don't count _settings as a combination
    }

    if (comboCount == 0)
    {
        Logger::getInstance().log("No combinations loaded!");
        return false;
    }

    // Log loaded combinations
    Logger::getInstance().log("Loaded combination set '" + currentPrefix + "_" + String(currentSetNumber) + "' (" + String(comboCount) + " entries):");
    for (JsonPair combo : combinations)
    {
        // Skip _settings in the combination listing
        if (String(combo.key().c_str()) == "_settings")
        {
            continue;
        }

        String logMessage = "  " + String(combo.key().c_str()) + ": ";

        for (JsonVariant action : combo.value().as<JsonArray>())
        {
            logMessage += String(action.as<const char *>()) + " ";
        }

        Logger::getInstance().log(logMessage);
    }

    return true;
}

bool CombinationManager::loadCombinations(int setNumber)
{
    return loadCombinationsInternal(setNumber, "combo");
}

bool CombinationManager::reloadCombinations(int setNumber, const char* prefix)
{
    Logger::getInstance().log("Reloading combinations: " + String(prefix) + "_" + String(setNumber));
    return loadCombinationsInternal(setNumber, prefix);
}

JsonObject CombinationManager::getCombinations()
{
    return combinations;
}
