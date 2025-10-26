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

// combinationManager.h
#ifndef COMBINATION_MANAGER_H
#define COMBINATION_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>

// Struct to hold combo settings
struct ComboSettings {
    int ledR = -1;  // -1 means not set
    int ledG = -1;
    int ledB = -1;

    bool hasLedColor() const { return ledR >= 0 && ledG >= 0 && ledB >= 0; }
};

class CombinationManager {
private:
    StaticJsonDocument<3072> doc;  // Reduced from 8192 to 3072 bytes (saves ~5KB RAM)
    JsonObject combinations;
    int currentSetNumber;  // Track current loaded set
    String currentPrefix;  // Track current file prefix ("combo" or "my_combo")
    ComboSettings settings;  // Settings for current combo set

    bool loadJsonFile(const char* filepath, JsonObject& target);
    bool mergeJsonFile(const char* filepath, JsonObject& target);
    bool loadCombinationsInternal(int setNumber, const char* prefix);
    void parseSettings(JsonObject& obj);

public:
    CombinationManager();
    bool loadCombinations(int setNumber = 0);
    bool reloadCombinations(int setNumber, const char* prefix = "combo");
    JsonObject getCombinations();
    int getCurrentSet() const { return currentSetNumber; }
    String getCurrentPrefix() const { return currentPrefix; }
    const ComboSettings& getSettings() const { return settings; }
};

#endif // COMBINATION_MANAGER_H
