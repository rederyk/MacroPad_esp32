/*
 * ESP32 MacroPad Project - Event Scheduler
 *
 * Executes programmable actions defined in config.json (scheduler section).
 */

#ifndef EVENT_SCHEDULER_H
#define EVENT_SCHEDULER_H

#include <Arduino.h>
#include <vector>
#include "configTypes.h"
#include "inputDevice.h"

class EventScheduler
{
public:
    EventScheduler();

    void begin(const SchedulerConfig &config);
    void reload(const SchedulerConfig &config);
    void update();

    void handleInputEvent(const InputEvent &event);
    void notifySensorEvent(const String &source, const String &name, float value = 0.0f);

    bool triggerEventById(const String &id, const char *reason = "manual");
    bool setManualTime(time_t epochSeconds, int timezoneMinutes = 0);

    bool hasValidTime() const;
    uint64_t getNextWakeDelayUs() const;
    bool shouldPreventSleep() const;

    String buildStatusJson() const;

private:
    struct RuntimeEvent
    {
        ScheduledActionConfig config;
        time_t nextEpoch{0};
        uint64_t nextIntervalMs{0};
        bool sensorPending{false};
        bool running{false};
        time_t lastRunEpoch{0};
        uint32_t executions{0};
        bool lastResult{false};
        String lastMessage;
        String lastReason;
    };

    SchedulerConfig currentConfig;
    std::vector<RuntimeEvent> runtimeEvents;
    uint32_t lastUpdateMs{0};

    void rebuildRuntimeEvents();
    void scheduleNext(RuntimeEvent &evt, time_t now, uint64_t nowMs, bool initial);
    bool shouldFire(const RuntimeEvent &evt, time_t now, uint64_t nowMs) const;
    bool executeEvent(RuntimeEvent &evt, const char *reason);
    bool executeSpecialAction(RuntimeEvent &evt);
    time_t computeNextTimeOfDay(const ScheduledActionConfig &cfg, time_t now) const;
    bool timeAvailable(time_t now) const;
};

#endif // EVENT_SCHEDULER_H
