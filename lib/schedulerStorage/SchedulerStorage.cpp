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

#include "SchedulerStorage.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "Logger.h"
#include "FileSystemManager.h"

#include "SchedulerStorage.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "Logger.h"
#include "FileSystemManager.h"

SchedulerStorage::SchedulerStorage() {}

bool SchedulerStorage::loadConfig(SchedulerConfig* config) {
    if (!FileSystemManager::ensureMounted()) {
        Logger::getInstance().log("Failed to mount LittleFS");
        return false;
    }

    File configFile = LittleFS.open("/scheduler.json");
    if (!configFile) {
        Logger::getInstance().log("Failed to open scheduler.json");
        return false;
    }

    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close();

    if (error) {
        Logger::getInstance().log("Failed to parse scheduler.json: " + String(error.c_str()));
        return false;
    }

    JsonArray eventsArray = doc.as<JsonArray>();
    parseSchedulerConfig(config, eventsArray);

    return true;
}

void SchedulerStorage::parseSchedulerConfig(SchedulerConfig* config, JsonArray& eventsArray) {
    config->events.clear();
    if (eventsArray.isNull()) {
        return;
    }

    config->events.reserve(eventsArray.size());
    for (JsonObject eventObj : eventsArray) {
        ScheduledActionConfig eventConfig;
        eventConfig.id = eventObj["id"] | "";
        if (eventConfig.id.isEmpty()) {
            continue;
        }

        eventConfig.description = eventObj["description"] | "";
        eventConfig.enabled = eventObj["enabled"] | true;
        eventConfig.wakeFromSleep = eventObj["wake_from_sleep"] | false;
        eventConfig.preventSleep = eventObj["prevent_sleep"] | false;
        eventConfig.runOnBoot = eventObj["run_on_boot"] | false;
        eventConfig.oneShot = eventObj["one_shot"] | false;
        eventConfig.allowOverlap = eventObj["allow_overlap"] | false;

        JsonObject triggerObj = eventObj["trigger"].as<JsonObject>();
        if (triggerObj.isNull()) {
            continue;
        }

        eventConfig.trigger.type = parseTriggerType(triggerObj["type"] | "interval");
        eventConfig.trigger.intervalMs = triggerObj["interval_ms"] | 0;
        eventConfig.trigger.jitterMs = triggerObj["jitter_ms"] | 0;
        eventConfig.trigger.absoluteEpoch = triggerObj["epoch"] | (time_t)0;
        eventConfig.trigger.hour = triggerObj["hour"] | 0;
        eventConfig.trigger.minute = triggerObj["minute"] | 0;
        eventConfig.trigger.second = triggerObj["second"] | 0;
        eventConfig.trigger.daysMask = parseDaysMask(triggerObj["days"]);
        eventConfig.trigger.useUtc = triggerObj["use_utc"] | false;
        eventConfig.trigger.inputSource = triggerObj["source"] | "";
        eventConfig.trigger.inputType = triggerObj["event"] | "";
        eventConfig.trigger.inputValue = triggerObj["value"] | -1;
        if (triggerObj.containsKey("state")) {
            eventConfig.trigger.inputState = triggerObj["state"].as<bool>() ? 1 : 0;
        } else {
            eventConfig.trigger.inputState = -1;
        }
        eventConfig.trigger.inputText = triggerObj["text"] | "";

        JsonObject actionObj = eventObj["action"].as<JsonObject>();
        if (actionObj.isNull()) {
            continue;
        }
        eventConfig.actionType = actionObj["type"] | "special_action";
        eventConfig.actionId = actionObj["id"] | "";
        if (actionObj.containsKey("params")) {
            String paramsJson;
            serializeJson(actionObj["params"], paramsJson);
            eventConfig.actionParams = paramsJson;
        } else {
            eventConfig.actionParams = "";
        }

        config->events.push_back(eventConfig);
    }
}

int SchedulerStorage::dayNameToIndex(String name) {
    name.toLowerCase();
    name.trim();
    if (name == "sun" || name == "sunday" || name == "dom" || name == "domenica" || name == "0")
        return 0;
    if (name == "mon" || name == "monday" || name == "lun" || name == "lunedi" || name == "1")
        return 1;
    if (name == "tue" || name == "tuesday" || name == "mar" || name == "martedi" || name == "2")
        return 2;
    if (name == "wed" || name == "wednesday" || name == "mer" || name == "mercoledi" || name == "3")
        return 3;
    if (name == "thu" || name == "thursday" || name == "gio" || name == "giovedi" || name == "4")
        return 4;
    if (name == "fri" || name == "friday" || name == "ven" || name == "venerdi" || name == "5")
        return 5;
    if (name == "sat" || name == "saturday" || name == "sab" || name == "sabato" || name == "6")
        return 6;
    if (name == "all" || name == "daily" || name == "*" || name == "everyday")
        return 7;
    if (name == "weekdays")
        return 8;
    if (name == "weekend")
        return 9;
    return -1;
}

void SchedulerStorage::addDayTokenMask(const String &token, uint8_t &mask) {
    int idx = dayNameToIndex(token);
    if (idx >= 0 && idx < 7) {
        mask |= (1 << idx);
    } else if (idx == 7) {
        mask = 0x7F;
    } else if (idx == 8) {
        mask |= (0b0111110); // Monday-Friday bits 1-5
    } else if (idx == 9) {
        mask |= (1 << 0) | (1 << 6);
    }
}

uint8_t SchedulerStorage::parseDaysMask(JsonVariantConst variant) {
    uint8_t mask = 0;

    if (variant.is<JsonArrayConst>()) {
        for (JsonVariantConst entry : variant.as<JsonArrayConst>()) {
            if (entry.is<int>()) {
                int idx = entry.as<int>();
                if (idx >= 0 && idx < 7) {
                    mask |= (1 << idx);
                }
            } else if (entry.is<const char *>()) {
                addDayTokenMask(String(entry.as<const char *>()), mask);
            }
        }
    } else if (variant.is<const char *>()) {
        String text = variant.as<const char *>();
        int start = 0;
        while (start < text.length()) {
            int end = text.indexOf(',', start);
            if (end == -1) {
                end = text.length();
            }
            String token = text.substring(start, end);
            token.trim();
            if (token.length() > 0) {
                addDayTokenMask(token, mask);
            }
            start = end + 1;
        }
    } else if (variant.is<int>()) {
        int idx = variant.as<int>();
        if (idx >= 0 && idx < 7) {
            mask |= (1 << idx);
        }
    }

    if (mask == 0) {
        mask = 0x7F;
    }
    return mask;
}

ScheduleTriggerType SchedulerStorage::parseTriggerType(const String &typeStr) {
    String lowered = typeStr;
    lowered.toLowerCase();
    lowered.trim();
    if (lowered == "time" || lowered == "time_of_day" || lowered == "daily" || lowered == "clock") {
        return ScheduleTriggerType::TIME_OF_DAY;
    }
    if (lowered == "interval" || lowered == "every" || lowered == "loop") {
        return ScheduleTriggerType::INTERVAL;
    }
    if (lowered == "absolute" || lowered == "once" || lowered == "epoch") {
        return ScheduleTriggerType::ABSOLUTE_TIME;
    }
    if (lowered == "input" || lowered == "sensor" || lowered == "event") {
        return ScheduleTriggerType::INPUT_EVENT;
    }
    return ScheduleTriggerType::NONE;
}
