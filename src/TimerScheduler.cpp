//
// Timer Scheduler implementation
//

#include "TimerScheduler.h"
#include "ShowController.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

TimerScheduler::TimerScheduler(Config::ConfigManager &config, ShowController &showController)
    : config(config), showController(showController), ntpAvailable(false) {
}

void TimerScheduler::begin() {
    timersConfig = config.loadTimersConfig();
#ifdef ARDUINO
    Serial.printf("TimerScheduler: Loaded %d timers, timezone offset: %d hours\n",
                  Config::TimersConfig::MAX_TIMERS, timersConfig.timezone_offset_hours);

    for (uint8_t i = 0; i < Config::TimersConfig::MAX_TIMERS; i++) {
        if (timersConfig.timers[i].enabled) {
            Serial.printf("  Timer %d: type=%d, action=%d, target=%lu\n",
                          i, static_cast<int>(timersConfig.timers[i].type),
                          static_cast<int>(timersConfig.timers[i].action),
                          timersConfig.timers[i].target_time);
        }
    }
#endif
}

uint32_t TimerScheduler::getSecondsSinceMidnight(uint32_t epochTime) const {
    if (epochTime == 0) return 0;

    // Apply timezone offset
    int32_t localEpoch = static_cast<int32_t>(epochTime) + (timersConfig.timezone_offset_hours * 3600);
    
    // Ensure we handle negative localEpoch (though unlikely with epoch times)
    if (localEpoch < 0) {
        return (86400 + (localEpoch % 86400)) % 86400;
    }

    // Calculate seconds since midnight in local time
    return static_cast<uint32_t>(localEpoch) % 86400;
}

void TimerScheduler::checkTimers(uint32_t currentEpoch) {
    if (!ntpAvailable || currentEpoch == 0) {
        return; // Skip timer checks if NTP is not available
    }

    uint32_t currentSecondsSinceMidnight = getSecondsSinceMidnight(currentEpoch);
    bool configChanged = false;

    for (uint8_t i = 0; i < Config::TimersConfig::MAX_TIMERS; i++) {
        Config::TimerEntry &timer = timersConfig.timers[i];

        if (!timer.enabled) {
            continue;
        }

        bool shouldTrigger = false;

        switch (timer.type) {
            case Config::TimerType::COUNTDOWN:
            case Config::TimerType::ALARM_ONCE:
                // Check if current epoch >= target epoch
                if (currentEpoch >= timer.target_time) {
                    shouldTrigger = true;
                    timer.enabled = false; // One-shot timers are disabled after triggering
                    configChanged = true;
                }
                break;

            case Config::TimerType::ALARM_DAILY:
                // Check if current time matches target time (within 1 second window)
                // target_time stores seconds since midnight for daily alarms
                if (currentSecondsSinceMidnight >= timer.target_time &&
                    currentSecondsSinceMidnight < timer.target_time + 5) {
                    shouldTrigger = true;
                    // Daily alarms remain enabled but we need to prevent multiple triggers
                    // Use duration_seconds as a "last triggered minute" marker
                    // We need to store seconds since midnight of the trigger time to be robust
                    // but currentEpoch / 60 is also okay as long as it's consistent.
                    uint32_t triggerMinute = currentEpoch / 60;
                    if (timer.duration_seconds != triggerMinute) {
                        timer.duration_seconds = triggerMinute;
                        configChanged = true;
                    } else {
                        shouldTrigger = false; // Already triggered this minute
                    }
                }
                break;
        }

        if (shouldTrigger) {
            executeTimer(i);
        }
    }

    if (configChanged) {
        config.saveTimersConfig(timersConfig);
    }
}

void TimerScheduler::executeTimer(uint8_t index) {
#ifdef ARDUINO
    const Config::TimerEntry &timer = timersConfig.timers[index];

    Serial.printf("TimerScheduler: Executing timer %d, action=%d\n",
                  index, static_cast<int>(timer.action));

    switch (timer.action) {
        case Config::TimerAction::TURN_OFF:
            // Turn off LEDs by switching to Solid show with black color
            showController.queueShowChange("Solid", R"({"colors":[[0,0,0]]})");
            Serial.println("TimerScheduler: LEDs turned off");
            break;

        case Config::TimerAction::LOAD_PRESET:
            // Load the specified preset
            {
                Config::PresetsConfig presetsConfig = config.loadPresetsConfig();
                if (timer.preset_index < Config::PresetsConfig::MAX_PRESETS &&
                    presetsConfig.presets[timer.preset_index].valid) {
                    showController.queuePresetLoad(presetsConfig.presets[timer.preset_index]);
                    Serial.printf("TimerScheduler: Loaded preset %d (%s)\n",
                                  timer.preset_index, presetsConfig.presets[timer.preset_index].name);
                } else {
                    Serial.printf("TimerScheduler: Preset %d is invalid, cancelling timer\n",
                                  timer.preset_index);
                }
            }
            break;
    }
#endif
}

bool TimerScheduler::setCountdown(uint8_t index, uint32_t durationSeconds,
                                   Config::TimerAction action, uint8_t presetIndex,
                                   uint32_t currentEpoch) {
    if (index >= Config::TimersConfig::MAX_TIMERS) {
        return false;
    }

    Config::TimerEntry &timer = timersConfig.timers[index];
    timer.enabled = true;
    timer.type = Config::TimerType::COUNTDOWN;
    timer.action = action;
    timer.preset_index = presetIndex;
    timer.target_time = currentEpoch + durationSeconds;
    timer.duration_seconds = durationSeconds;

    config.saveTimersConfig(timersConfig);

#ifdef ARDUINO
    Serial.printf("TimerScheduler: Set countdown timer %d for %lu seconds\n", index, durationSeconds);
#endif

    return true;
}

bool TimerScheduler::setAlarmOnce(uint8_t index, uint32_t epochTime,
                                   Config::TimerAction action, uint8_t presetIndex) {
    if (index >= Config::TimersConfig::MAX_TIMERS) {
        return false;
    }

    Config::TimerEntry &timer = timersConfig.timers[index];
    timer.enabled = true;
    timer.type = Config::TimerType::ALARM_ONCE;
    timer.action = action;
    timer.preset_index = presetIndex;
    timer.target_time = epochTime;
    timer.duration_seconds = 0;

    config.saveTimersConfig(timersConfig);

#ifdef ARDUINO
    Serial.printf("TimerScheduler: Set one-shot alarm %d for epoch %lu\n", index, epochTime);
#endif

    return true;
}

bool TimerScheduler::setDailyAlarm(uint8_t index, uint32_t secondsSinceMidnight,
                                    Config::TimerAction action, uint8_t presetIndex) {
    if (index >= Config::TimersConfig::MAX_TIMERS) {
        return false;
    }

    if (secondsSinceMidnight >= 86400) {
        return false; // Invalid time
    }

    Config::TimerEntry &timer = timersConfig.timers[index];
    timer.enabled = true;
    timer.type = Config::TimerType::ALARM_DAILY;
    timer.action = action;
    timer.preset_index = presetIndex;
    timer.target_time = secondsSinceMidnight;
    timer.duration_seconds = 0; // Will be used to track last trigger time

    config.saveTimersConfig(timersConfig);

#ifdef ARDUINO
    uint8_t hours = secondsSinceMidnight / 3600;
    uint8_t minutes = (secondsSinceMidnight % 3600) / 60;
    Serial.printf("TimerScheduler: Set daily alarm %d for %02d:%02d\n", index, hours, minutes);
#endif

    return true;
}

bool TimerScheduler::cancelTimer(uint8_t index) {
    if (index >= Config::TimersConfig::MAX_TIMERS) {
        return false;
    }

    timersConfig.timers[index].enabled = false;
    config.saveTimersConfig(timersConfig);

#ifdef ARDUINO
    Serial.printf("TimerScheduler: Cancelled timer %d\n", index);
#endif

    return true;
}

uint32_t TimerScheduler::getRemainingSeconds(uint8_t index, uint32_t currentEpoch) const {
    if (index >= Config::TimersConfig::MAX_TIMERS) {
        return 0;
    }

    const Config::TimerEntry &timer = timersConfig.timers[index];

    if (!timer.enabled) {
        return 0;
    }

    switch (timer.type) {
        case Config::TimerType::COUNTDOWN:
        case Config::TimerType::ALARM_ONCE:
            if (currentEpoch >= timer.target_time) {
                return 0;
            }
            return timer.target_time - currentEpoch;

        case Config::TimerType::ALARM_DAILY:
            // For daily alarms, calculate time until next occurrence
            {
                uint32_t currentSecondsSinceMidnight = getSecondsSinceMidnight(currentEpoch);
                if (currentSecondsSinceMidnight < timer.target_time) {
                    return timer.target_time - currentSecondsSinceMidnight;
                } else {
                    // Timer will trigger tomorrow
                    return (86400 - currentSecondsSinceMidnight) + timer.target_time;
                }
            }
    }

    return 0;
}

void TimerScheduler::setTimezoneOffset(int8_t offsetHours) {
    if (offsetHours < -12 || offsetHours > 14) {
        return; // Invalid offset
    }

    timersConfig.timezone_offset_hours = offsetHours;
    config.saveTimersConfig(timersConfig);

#ifdef ARDUINO
    Serial.printf("TimerScheduler: Set timezone offset to %d hours\n", offsetHours);
#endif
}
