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

#ifndef SCHEDULER_STORAGE_H
#define SCHEDULER_STORAGE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "configTypes.h"

class SchedulerStorage {
public:
    SchedulerStorage();

    // Loads the scheduler events from /scheduler.json and merges them into the provided config
    bool loadConfig(SchedulerConfig* config);

private:
    // Private methods for parsing JSON
    void parseSchedulerConfig(SchedulerConfig* config, JsonArray& doc);
    uint8_t parseDaysMask(JsonVariantConst variant);
    ScheduleTriggerType parseTriggerType(const String& typeStr);
    void addDayTokenMask(const String &token, uint8_t &mask);
    int dayNameToIndex(String name);
};

#endif // SCHEDULER_STORAGE_H
