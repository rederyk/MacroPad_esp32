/*
 * EventScheduler.cpp
 *
 * Executes programmable events defined in config.json (scheduler section).
 */

#include "EventScheduler.h"

#include <ArduinoJson.h>
#include <time.h>
#include <esp_system.h>
#include <sys/time.h>
#include "Logger.h"
#include "SpecialActionRouter.h"
#include "powerManager.h"

extern PowerManager powerManager;

namespace
{
constexpr time_t kEpochThreshold = 1609459200; // 2021-01-01

uint32_t randomWithin(uint32_t limit)
{
    if (limit == 0)
    {
        return 0;
    }
    return esp_random() % limit;
}

String eventTypeToString(InputEvent::EventType type)
{
    switch (type)
    {
    case InputEvent::EventType::KEY_PRESS:
        return "key";
    case InputEvent::EventType::ROTATION:
        return "rotation";
    case InputEvent::EventType::BUTTON:
        return "button";
    case InputEvent::EventType::MOTION:
        return "motion";
    default:
        return "unknown";
    }
}
} // namespace

EventScheduler::EventScheduler() : lastUpdateMs(0) {}

void EventScheduler::begin(const SchedulerConfig &config)
{
    currentConfig = config;
    rebuildRuntimeEvents();
}

void EventScheduler::reload(const SchedulerConfig &config)
{
    currentConfig = config;
    rebuildRuntimeEvents();
}

void EventScheduler::rebuildRuntimeEvents()
{
    runtimeEvents.clear();
    if (!currentConfig.enabled || currentConfig.events.empty())
    {
        return;
    }

    runtimeEvents.reserve(currentConfig.events.size());
    const time_t now = time(nullptr);
    const uint64_t nowMs = millis();

    for (const auto &cfg : currentConfig.events)
    {
        RuntimeEvent evt;
        evt.config = cfg;
        scheduleNext(evt, now, nowMs, true);
        runtimeEvents.push_back(evt);
    }

    Logger::getInstance().log("Scheduler initialized with " + String(runtimeEvents.size()) + " events");
}

void EventScheduler::scheduleNext(RuntimeEvent &evt, time_t now, uint64_t nowMs, bool initial)
{
    if (!evt.config.enabled)
    {
        evt.nextEpoch = 0;
        evt.nextIntervalMs = 0;
        evt.sensorPending = false;
        return;
    }

    evt.sensorPending = false;

    switch (evt.config.trigger.type)
    {
    case ScheduleTriggerType::TIME_OF_DAY:
        evt.nextEpoch = computeNextTimeOfDay(evt.config, now);
        break;
    case ScheduleTriggerType::ABSOLUTE_TIME:
        evt.nextEpoch = evt.config.trigger.absoluteEpoch;
        break;
    case ScheduleTriggerType::INTERVAL:
    {
        uint32_t interval = evt.config.trigger.intervalMs > 0 ? evt.config.trigger.intervalMs : 1000;
        uint32_t jitter = evt.config.trigger.jitterMs;
        evt.nextIntervalMs = nowMs + interval + randomWithin(jitter);
        if (initial && evt.config.runOnBoot)
        {
            evt.nextIntervalMs = nowMs;
        }
        break;
    }
    case ScheduleTriggerType::INPUT_EVENT:
        evt.nextEpoch = 0;
        evt.nextIntervalMs = 0;
        if (initial && evt.config.runOnBoot)
        {
            evt.sensorPending = true;
            evt.lastReason = "startup";
        }
        break;
    default:
        evt.nextEpoch = 0;
        evt.nextIntervalMs = 0;
        break;
    }

    if (initial && evt.config.runOnBoot &&
        evt.config.trigger.type != ScheduleTriggerType::INTERVAL &&
        evt.config.trigger.type != ScheduleTriggerType::INPUT_EVENT)
    {
        evt.nextEpoch = now;
    }
}

bool EventScheduler::timeAvailable(time_t now) const
{
    return now >= kEpochThreshold;
}

void EventScheduler::update()
{
    if (!currentConfig.enabled || runtimeEvents.empty())
    {
        return;
    }

    const uint64_t nowMs = millis();
    if (currentConfig.pollIntervalMs > 0 && lastUpdateMs != 0)
    {
        const uint32_t elapsed = static_cast<uint32_t>(nowMs - lastUpdateMs);
        if (elapsed < currentConfig.pollIntervalMs)
        {
            return;
        }
    }
    lastUpdateMs = nowMs;

    const time_t now = time(nullptr);

    for (auto &evt : runtimeEvents)
    {
        if (!evt.config.enabled)
        {
            continue;
        }

        if ((evt.config.trigger.type == ScheduleTriggerType::TIME_OF_DAY ||
             evt.config.trigger.type == ScheduleTriggerType::ABSOLUTE_TIME) &&
            !timeAvailable(now))
        {
            continue;
        }

        if (!shouldFire(evt, now, nowMs))
        {
            continue;
        }

        executeEvent(evt, evt.lastReason.isEmpty() ? "scheduler" : evt.lastReason.c_str());

        if (evt.config.oneShot)
        {
            evt.config.enabled = false;
            evt.nextEpoch = 0;
            evt.nextIntervalMs = 0;
            continue;
        }

        scheduleNext(evt, now, nowMs, false);
    }
}

bool EventScheduler::shouldFire(const RuntimeEvent &evt, time_t now, uint64_t nowMs) const
{
    switch (evt.config.trigger.type)
    {
    case ScheduleTriggerType::TIME_OF_DAY:
    case ScheduleTriggerType::ABSOLUTE_TIME:
        return evt.nextEpoch > 0 && now >= evt.nextEpoch;
    case ScheduleTriggerType::INTERVAL:
        return evt.nextIntervalMs > 0 && nowMs >= evt.nextIntervalMs;
    case ScheduleTriggerType::INPUT_EVENT:
        return evt.sensorPending;
    default:
        return false;
    }
}

bool EventScheduler::executeEvent(RuntimeEvent &evt, const char *reason)
{
    evt.running = true;
    evt.sensorPending = false;
    evt.lastReason = reason ? reason : "";

    Logger::getInstance().log("Scheduler executing '" + evt.config.id + "' (" + evt.lastReason + ")");

    bool success = false;
    if (evt.config.actionType == "special_action")
    {
        success = executeSpecialAction(evt);
    }
    else if (evt.config.actionType == "log")
    {
        Logger::getInstance().log(evt.config.actionId);
        evt.lastMessage = evt.config.actionId;
        success = true;
    }
    else if (evt.config.actionType == "sleep" && evt.config.actionId == "enter")
    {
        evt.lastMessage = "Entering sleep";
        success = true;
        powerManager.enterDeepSleep(true);
    }
    else
    {
        evt.lastMessage = "Unsupported action type";
        Logger::getInstance().log("Scheduler: unsupported action type '" + evt.config.actionType + "'");
    }

    evt.lastRunEpoch = time(nullptr);
    evt.executions++;
    evt.lastResult = success;
    evt.running = false;
    return success;
}

bool EventScheduler::executeSpecialAction(RuntimeEvent &evt)
{
    StaticJsonDocument<256> paramsDoc;
    JsonVariantConst paramsVariant;

    if (evt.config.actionParams.length() > 0)
    {
        DeserializationError err = deserializeJson(paramsDoc, evt.config.actionParams);
        if (err)
        {
            Logger::getInstance().log("Scheduler failed to parse params for '" + evt.config.id + "': " + String(err.c_str()));
        }
        else
        {
            paramsVariant = paramsDoc.as<JsonVariantConst>();
        }
    }

    int statusCode = 200;
    String message;
    const bool ok = handleSpecialActionRequest(evt.config.actionId, paramsVariant, message, statusCode);
    evt.lastMessage = message;
    if (!ok)
    {
        Logger::getInstance().log("Scheduler action '" + evt.config.id + "' failed (" + String(statusCode) + "): " + message);
    }
    return ok;
}

time_t EventScheduler::computeNextTimeOfDay(const ScheduledActionConfig &cfg, time_t now) const
{
    if (!timeAvailable(now))
    {
        return 0;
    }

    const int tzOffsetSeconds = cfg.trigger.useUtc ? 0 : currentConfig.timezoneOffsetMinutes * 60;
    time_t localNow = now + tzOffsetSeconds;

    struct tm localInfo;
    gmtime_r(&localNow, &localInfo);

    const int secondsIntoDay = (localInfo.tm_hour * 3600) + (localInfo.tm_min * 60) + localInfo.tm_sec;
    time_t dayStart = localNow - secondsIntoDay;
    const int targetSeconds = (cfg.trigger.hour * 3600) + (cfg.trigger.minute * 60) + cfg.trigger.second;

    time_t candidateLocal = dayStart + targetSeconds;
    uint8_t dayMask = cfg.trigger.daysMask == 0 ? 0x7F : cfg.trigger.daysMask;
    int dayOffset = 0;

    while (true)
    {
        int candidateDay = (localInfo.tm_wday + dayOffset) % 7;
        bool dayAllowed = (dayMask & (1 << candidateDay)) != 0;
        bool timePassed = (dayOffset == 0 && candidateLocal <= localNow);

        if (dayAllowed && !timePassed)
        {
            break;
        }

        candidateLocal += 86400;
        dayOffset++;

        if (dayOffset > 7)
        {
            // Fallback: if no days are enabled, break to avoid infinite loop
            break;
        }
    }

    return candidateLocal - tzOffsetSeconds;
}

void EventScheduler::handleInputEvent(const InputEvent &event)
{
    if (!currentConfig.enabled)
    {
        return;
    }

    const String eventType = eventTypeToString(event.type);

    for (auto &evt : runtimeEvents)
    {
        if (!evt.config.enabled || evt.config.trigger.type != ScheduleTriggerType::INPUT_EVENT)
        {
            continue;
        }

        if (!evt.config.trigger.inputType.isEmpty() &&
            !evt.config.trigger.inputType.equalsIgnoreCase(eventType))
        {
            continue;
        }

        if (evt.config.trigger.inputValue >= 0 &&
            evt.config.trigger.inputValue != event.value1)
        {
            continue;
        }

        if (!evt.config.trigger.inputText.isEmpty() &&
            evt.config.trigger.inputText != event.text)
        {
            continue;
        }

        if (evt.config.trigger.inputState != -1)
        {
            bool desired = evt.config.trigger.inputState == 1;
            if (event.state != desired)
            {
                continue;
            }
        }

        evt.sensorPending = true;
        evt.lastReason = "input:" + eventType;
    }
}

void EventScheduler::notifySensorEvent(const String &source, const String &name, float value)
{
    if (!currentConfig.enabled)
    {
        return;
    }

    for (auto &evt : runtimeEvents)
    {
        if (!evt.config.enabled || evt.config.trigger.type != ScheduleTriggerType::INPUT_EVENT)
        {
            continue;
        }

        if (!evt.config.trigger.inputSource.isEmpty() &&
            !evt.config.trigger.inputSource.equalsIgnoreCase(source))
        {
            continue;
        }

        if (!evt.config.trigger.inputType.isEmpty() &&
            !evt.config.trigger.inputType.equalsIgnoreCase(name))
        {
            continue;
        }

        evt.sensorPending = true;
        evt.lastReason = "sensor:" + source;
    }
}

bool EventScheduler::triggerEventById(const String &id, const char *reason)
{
    for (auto &evt : runtimeEvents)
    {
        if (evt.config.id == id && evt.config.enabled)
        {
            return executeEvent(evt, reason ? reason : "manual");
        }
    }
    return false;
}

bool EventScheduler::setManualTime(time_t epochSeconds, int timezoneMinutes)
{
    if (epochSeconds <= 0)
    {
        return false;
    }

    timeval tv{};
    tv.tv_sec = epochSeconds;
    tv.tv_usec = 0;
    if (settimeofday(&tv, nullptr) != 0)
    {
        return false;
    }

    currentConfig.timezoneOffsetMinutes = timezoneMinutes;
    rebuildRuntimeEvents();
    return true;
}

bool EventScheduler::hasValidTime() const
{
    return timeAvailable(time(nullptr));
}

uint64_t EventScheduler::getNextWakeDelayUs() const
{
    if (!currentConfig.enabled)
    {
        return 0;
    }

    const time_t now = time(nullptr);
    const uint64_t nowMs = millis();
    uint64_t bestUs = 0;

    for (const auto &evt : runtimeEvents)
    {
        if (!evt.config.enabled || !evt.config.wakeFromSleep)
        {
            continue;
        }

        uint64_t candidateUs = 0;
        if ((evt.config.trigger.type == ScheduleTriggerType::TIME_OF_DAY ||
             evt.config.trigger.type == ScheduleTriggerType::ABSOLUTE_TIME) &&
            timeAvailable(now) && evt.nextEpoch > 0)
        {
            long delta = static_cast<long>(evt.nextEpoch - now);
            if (delta < 0)
            {
                delta = 0;
            }
            candidateUs = static_cast<uint64_t>(delta) * 1000000ULL;
        }
        else if (evt.config.trigger.type == ScheduleTriggerType::INTERVAL &&
                 evt.nextIntervalMs > 0)
        {
            uint64_t deltaMs = evt.nextIntervalMs > nowMs ? evt.nextIntervalMs - nowMs : 0;
            candidateUs = deltaMs * 1000ULL;
        }

        if (candidateUs == 0)
        {
            continue;
        }

        if (bestUs == 0 || candidateUs < bestUs)
        {
            bestUs = candidateUs;
        }
    }

    return bestUs;
}

bool EventScheduler::shouldPreventSleep() const
{
    if (!currentConfig.enabled || !currentConfig.preventSleepIfPending)
    {
        return false;
    }

    const time_t now = time(nullptr);
    const uint64_t nowMs = millis();

    for (const auto &evt : runtimeEvents)
    {
        if (!evt.config.enabled || !evt.config.preventSleep)
        {
            continue;
        }

        if (evt.config.trigger.type == ScheduleTriggerType::INPUT_EVENT && evt.sensorPending)
        {
            return true;
        }

        if ((evt.config.trigger.type == ScheduleTriggerType::TIME_OF_DAY ||
             evt.config.trigger.type == ScheduleTriggerType::ABSOLUTE_TIME) &&
            timeAvailable(now) && evt.nextEpoch > 0)
        {
            long delta = static_cast<long>(evt.nextEpoch - now);
            if (delta <= static_cast<long>(currentConfig.sleepGuardSeconds))
            {
                return true;
            }
        }

        if (evt.config.trigger.type == ScheduleTriggerType::INTERVAL && evt.nextIntervalMs > 0)
        {
            uint64_t deltaMs = evt.nextIntervalMs > nowMs ? evt.nextIntervalMs - nowMs : 0;
            if (deltaMs <= static_cast<uint64_t>(currentConfig.sleepGuardSeconds) * 1000ULL)
            {
                return true;
            }
        }
    }

    return false;
}

String EventScheduler::buildStatusJson() const
{
    StaticJsonDocument<2048> doc;
    doc["enabled"] = currentConfig.enabled;
    doc["time_valid"] = hasValidTime();
    doc["timezone_minutes"] = currentConfig.timezoneOffsetMinutes;
    doc["sleep_guard_seconds"] = currentConfig.sleepGuardSeconds;
    doc["wake_ahead_seconds"] = currentConfig.wakeAheadSeconds;

    JsonArray eventsArr = doc.createNestedArray("events");

    const time_t now = time(nullptr);
    const uint64_t nowMs = millis();

    for (const auto &evt : runtimeEvents)
    {
        JsonObject obj = eventsArr.createNestedObject();
        obj["id"] = evt.config.id;
        obj["description"] = evt.config.description;
        obj["enabled"] = evt.config.enabled;
        obj["trigger"] = static_cast<int>(evt.config.trigger.type);
        obj["prevent_sleep"] = evt.config.preventSleep;
        obj["wake_from_sleep"] = evt.config.wakeFromSleep;
        obj["pending"] = evt.sensorPending;
        obj["executions"] = evt.executions;
        obj["last_success"] = evt.lastResult;
        obj["last_message"] = evt.lastMessage;
        obj["last_reason"] = evt.lastReason;
        obj["last_run_epoch"] = (int64_t)evt.lastRunEpoch;

        if (evt.nextEpoch > 0)
        {
            obj["next_epoch"] = (int64_t)evt.nextEpoch;
            if (timeAvailable(now))
            {
                obj["seconds_to_next"] = static_cast<long>(evt.nextEpoch - now);
            }
        }
        if (evt.nextIntervalMs > 0)
        {
            uint64_t deltaMs = evt.nextIntervalMs > nowMs ? evt.nextIntervalMs - nowMs : 0;
            obj["next_interval_ms"] = (uint64_t)evt.nextIntervalMs;
            obj["ms_to_next"] = deltaMs;
        }
    }

    String output;
    serializeJson(doc, output);
    return output;
}
