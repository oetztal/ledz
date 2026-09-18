//
// Timer Scheduler for ledz
// Manages countdown timers and schedules
//

#ifndef LEDZ_TIMER_SCHEDULER_H
#define LEDZ_TIMER_SCHEDULER_H

#include "Config.h"

// Forward declarations
class ShowController;

class TimerScheduler {
private:
    Config::ConfigManager &config;
    ShowController &showController;
    Config::TimersConfig timersConfig;
    bool ntpAvailable = false;
    // Set by setTimezone() on whatever task handled the request, consumed by
    // checkTimers() on the Network task. Applying TZ is a process-global
    // mutation, so only one task may do it — see design decision 4.
    bool tzDirty = false;

    /**
     * Execute a timer action
     */
    void executeTimer(uint8_t index);

public:
    /**
     * Get seconds since midnight for the given epoch time
     */
    [[nodiscard]] uint32_t getSecondsSinceMidnight(uint32_t epochTime) const;

    /**
     * Constructor
     * @param config Configuration manager reference
     * @param showController Show controller reference
     */
    TimerScheduler(Config::ConfigManager &config, ShowController &showController);

    /**
     * Initialize the scheduler - loads config from NVS
     */
    void begin();

    /**
     * Check all timers and execute any that have triggered
     * Called every second from Network task
     * @param currentEpoch Current NTP epoch time
     */
    void checkTimers(uint32_t currentEpoch);

    /**
     * Set NTP availability status
     * @param available true if NTP time is available
     */
    void setNtpAvailable(bool available) { ntpAvailable = available; }

    /**
     * Set a countdown timer
     * @param index Timer slot index (0 .. MAX_TIMERS-1)
     * @param durationSeconds Countdown duration in seconds
     * @param action Action to perform when timer expires
     * @param presetIndex Preset index (only used if action is LOAD_PRESET)
     * @param currentEpoch Current NTP epoch time
     * @return true if timer was set successfully
     */
    bool setCountdown(uint8_t index, uint32_t durationSeconds, Config::TimerAction action,
                      uint8_t presetIndex, uint32_t currentEpoch);

    /**
     * Set a schedule.
     *
     * Naming the index of a slot that already holds a schedule updates it in
     * place: time, action, preset and weekday mask are replaced, the slot
     * and its paused state are kept, and the schedule becomes eligible to fire
     * at its next occurrence, including later today.
     *
     * @param index Timer slot index (0 .. MAX_TIMERS-1)
     * @param secondsSinceMidnight Time of day as seconds since midnight
     * @param action Action to perform when schedule triggers
     * @param presetIndex Preset index (only used if action is LOAD_PRESET)
     * @param daysMask Weekdays to fire on, bit n = tm_wday n (Sunday = 0);
     *                 Config::SCHEDULE_EVERY_DAY for every day. 0 and values
     *                 above 0x7F are rejected.
     * @return true if schedule was set successfully
     */
    bool setSchedule(uint8_t index, uint32_t secondsSinceMidnight, Config::TimerAction action,
                       uint8_t presetIndex, uint8_t daysMask = Config::SCHEDULE_EVERY_DAY);

    /**
     * Pause or resume a schedule without changing any of its settings.
     * Does not touch the last-fired record, so resuming after today's
     * firing does not fire again today.
     * @param index Timer slot index (0 .. MAX_TIMERS-1)
     * @param paused true to pause, false to arm
     * @return false if the index is out of range, the slot is empty, or it
     *         holds a countdown timer
     */
    bool setPaused(uint8_t index, bool paused);

    /**
     * Cancel a timer
     * @param index Timer slot index (0 .. MAX_TIMERS-1)
     * @return true if timer was cancelled
     */
    bool cancelTimer(uint8_t index);

    /**
     * Get remaining seconds for a timer. For a schedule this is the wall-clock
     * time to its next occurrence on a selected weekday, looking at most
     * seven days ahead; a paused schedule reports 0.
     * @param index Timer slot index (0 .. MAX_TIMERS-1)
     * @param currentEpoch Current NTP epoch time
     * @return Remaining seconds, or 0 if timer is not active
     */
    [[nodiscard]] uint32_t getRemainingSeconds(uint8_t index, uint32_t currentEpoch) const;

    /**
     * Get the timers configuration (for API access)
     * @return Reference to timers configuration
     */
    [[nodiscard]] const Config::TimersConfig& getTimersConfig() const { return timersConfig; }

    /**
     * Set the timezone.
     *
     * Stores and persists the string, but does not apply it — the actual
     * setenv/tzset happens on the next checkTimers() iteration, so that a
     * request handler running on another task cannot mutate libc's timezone
     * state underneath a live schedule evaluation.
     *
     * @param tz POSIX TZ string, e.g. "CET-1CEST,M3.5.0,M10.5.0/3"
     * @return true if the string was accepted and stored
     */
    bool setTimezone(const char *tz);

    /**
     * Get the configured timezone
     * @return POSIX TZ string
     */
    [[nodiscard]] const char *getTimezone() const { return timersConfig.timezone; }
};

#endif //LEDZ_TIMER_SCHEDULER_H
