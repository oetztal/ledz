#include "TimerScheduler.h"
#include "Log.h"
#include "ShowController.h"
#include "support/LocalTime.h"

#include <cstdlib>
#include <cstring>

#ifdef ARDUINO
#include <Arduino.h>
#endif

static const char* TAG = "timer";

TimerScheduler::TimerScheduler(Config::ConfigManager &config, ShowController &showController)
    : config(config), showController(showController) {
}

void TimerScheduler::begin() {
    timersConfig = config.loadTimersConfig();
    // The stored zone still has to be handed to libc. Doing it through the
    // same dirty flag the API uses keeps the apply on the Network task and
    // guarantees it lands after configTime() has set TZ to UTC.
    tzDirty = true;
#ifdef ARDUINO
    ESP_LOGI(TAG, "Loaded %d timers, timezone: \"%s\"",
                  Config::TimersConfig::MAX_TIMERS, timersConfig.timezone);

    for (uint8_t i = 0; i < Config::TimersConfig::MAX_TIMERS; i++) {
        if (timersConfig.timers[i].enabled) {
            ESP_LOGI(TAG, "  Timer %d: type=%d, action=%d, target=%u",
                          i, static_cast<int>(timersConfig.timers[i].type),
                          static_cast<int>(timersConfig.timers[i].action),
                          timersConfig.timers[i].target_time);
        }
    }
#endif
}

uint32_t TimerScheduler::getSecondsSinceMidnight(uint32_t epochTime) const {
    return LocalTime::secondsSinceMidnight(epochTime, timersConfig.timezone);
}

void TimerScheduler::checkTimers(uint32_t currentEpoch) {
    if (!ntpAvailable || currentEpoch == 0) {
        return; // Skip timer checks if NTP is not available
    }

    if (tzDirty) {
        tzDirty = false;
        // A syntactically valid string can still be nonsense, and newlib
        // reports no parse error — it just falls back to UTC. Logging what
        // libc actually made of it is the only diagnostic there is.
        const LocalTime::Info info = LocalTime::describe(currentEpoch, timersConfig.timezone);
#ifdef ARDUINO
        ESP_LOGI(TAG, "Applied timezone \"%s\": %s, UTC%+d:%02d%s",
                      timersConfig.timezone, info.abbrev,
                      info.offset_minutes / 60, abs(info.offset_minutes % 60),
                      info.is_dst ? " (DST)" : "");
#else
        (void) info;
#endif
    }

    uint32_t currentSecondsSinceMidnight = getSecondsSinceMidnight(currentEpoch);
    const uint16_t today = LocalTime::localDayOfYear(currentEpoch, timersConfig.timezone);
    bool configChanged = false;

    for (uint8_t i = 0; i < Config::TimersConfig::MAX_TIMERS; i++) {
        Config::TimerEntry &timer = timersConfig.timers[i];

        if (!timer.enabled) {
            continue;
        }

        bool shouldTrigger = false;

        switch (timer.type) {
            case Config::TimerType::COUNTDOWN:
                // Check if current epoch >= target epoch
                if (currentEpoch >= timer.target_time) {
                    shouldTrigger = true;
                    timer.enabled = false; // One-shot timers are disabled after triggering
                    configChanged = true;
                }
                break;

            case Config::TimerType::ALARM_DAILY:
                // target_time stores seconds since midnight for daily alarms.
                // A wall-clock time that the spring-forward jump skips never
                // falls inside this window, so the alarm is skipped that day
                // and resumes the next — see design decision 8.
                if (currentSecondsSinceMidnight >= timer.target_time &&
                    currentSecondsSinceMidnight < timer.target_time + 5) {
                    // Daily alarms stay enabled, so repeat triggers are
                    // suppressed by recording the local day they last fired
                    // on. Keying on the local day rather than an absolute
                    // time is what makes the two 02:30s of a fall-back night
                    // count as one.
                    if (timer.last_fired_yday != today) {
                        timer.last_fired_yday = today;
                        shouldTrigger = true;
                        configChanged = true;
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

    ESP_LOGI(TAG, "Executing timer %d, action=%d",
                  index, static_cast<int>(timer.action));

    switch (timer.action) {
        case Config::TimerAction::TURN_OFF:
            // Turn off LEDs by switching to Solid show with black color
            showController.queueShowChange("Solid", R"({"colors":[[0,0,0]]})");
            ESP_LOGI(TAG, "LEDs turned off");
            break;

        case Config::TimerAction::LOAD_PRESET:
            // Load the specified preset
            {
                Config::PresetsConfig presetsConfig = config.loadPresetsConfig();
                if (timer.preset_index < Config::PresetsConfig::MAX_PRESETS &&
                    presetsConfig.presets[timer.preset_index].valid) {
                    showController.queuePresetLoad(presetsConfig.presets[timer.preset_index]);
                    ESP_LOGI(TAG, "Loaded preset %d (%s)",
                                  timer.preset_index, presetsConfig.presets[timer.preset_index].name);
                } else {
                    ESP_LOGW(TAG, "Preset %d is invalid, cancelling timer",
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
    ESP_LOGI(TAG, "Set countdown timer %d for %u seconds", index, durationSeconds);
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
    timer.duration_seconds = 0; // Unused by daily alarms
    // A freshly set alarm has never fired, so it is eligible today.
    timer.last_fired_yday = Config::ALARM_NEVER_FIRED;

    config.saveTimersConfig(timersConfig);

#ifdef ARDUINO
    uint8_t hours = secondsSinceMidnight / 3600;
    uint8_t minutes = (secondsSinceMidnight % 3600) / 60;
    ESP_LOGI(TAG, "Set daily alarm %d for %02d:%02d", index, hours, minutes);
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
    ESP_LOGI(TAG, "Cancelled timer %d", index);
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
            if (currentEpoch >= timer.target_time) {
                return 0;
            }
            return timer.target_time - currentEpoch;

        case Config::TimerType::ALARM_DAILY:
            // For daily alarms, calculate time until next occurrence.
            // This counts wall-clock seconds, not real ones, so on a day
            // with a transition the estimate is an hour out. It only drives
            // the UI countdown; the trigger itself compares wall-clock time
            // directly and is unaffected.
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

bool TimerScheduler::setTimezone(const char *tz) {
    if (!LocalTime::isSyntacticallyValidTz(tz)) {
        return false;
    }

    strncpy(timersConfig.timezone, tz, sizeof(timersConfig.timezone) - 1);
    timersConfig.timezone[sizeof(timersConfig.timezone) - 1] = '\0';
    config.saveTimersConfig(timersConfig);

    // Deliberately no setenv/tzset here: this runs on the request handler's
    // task. checkTimers() picks the change up within one iteration.
    tzDirty = true;

#ifdef ARDUINO
    ESP_LOGI(TAG, "Set timezone to \"%s\"", timersConfig.timezone);
#endif

    return true;
}
