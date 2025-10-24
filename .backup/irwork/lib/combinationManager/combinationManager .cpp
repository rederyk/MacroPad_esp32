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

bool CombinationManager::loadCombinations(int setNumber)
{

    if (!LittleFS.begin(true))
    {
        Logger::getInstance().log("Failed to mount LittleFS");
        return false;
    }

    File comboFile = LittleFS.open("/combo.json");
    if (!comboFile)
    {
        Logger::getInstance().log("Failed to open combo file");
        LittleFS.end();
        return false;
    }

    DeserializationError error = deserializeJson(doc, comboFile);
    if (error)
    {
        Logger::getInstance().log("Failed to parse combo file: " + String(error.c_str()));
        comboFile.close();
        LittleFS.end();
        return false;
    }

    comboFile.close();
    LittleFS.end();

    // Try to load the requested set
    String setKey = "combinations_" + String(setNumber);
    if (doc.containsKey(setKey))
    {
        combinations = doc[setKey];
    }
    else
    {
        // Fallback to set 0 if requested set doesn't exist
        Logger::getInstance().log("Set " + String(setNumber) + " not found, falling back to set 0");
        if (doc.containsKey("combinations_0"))
        {
            combinations = doc["combinations_0"];
        }
        else
        {
            Logger::getInstance().log("No combinations found in combo file");
            return false;
        }
    }

    // Log loaded combinations
    Logger::getInstance().log("Loaded combination set " + String(setNumber) + ":");
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
