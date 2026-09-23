#include "Config.h"
#include "Log.h"
#include "support/LocalTime.h"

#ifdef ARDUINO
#include <esp_system.h>
#endif

static const char* const TAG = "cfg";

namespace Config {
    ConfigManager::ConfigManager() = default;

    void ConfigManager::requestRestart(uint32_t delayMs) {
        restartAt = millis() + delayMs;
        restartRequested = true;
        ESP_LOGW(TAG, "Restart requested in %u ms", delayMs);
    }

    void ConfigManager::checkRestart() const {
        if (restartRequested && millis() >= restartAt) {
            ESP_LOGI(TAG, "Performing scheduled restart...");
#ifdef ARDUINO
            ESP.restart();
#endif
        }
    }

    void ConfigManager::begin() {
#ifdef ARDUINO
        // Preferences are initialized when needed in each method
        prefs.begin(NAMESPACE, true); // Read-only mode
#endif
    }

    bool ConfigManager::isConfigured() {
#ifdef ARDUINO
        prefs.begin(NAMESPACE, true); // Read-only mode
        bool configured = prefs.getBool("configured", false);
        prefs.end();
        return configured;
#else
        return false;
#endif
    }

    void ConfigManager::markUnconfigured() {
        prefs.begin(NAMESPACE, false); // Read-write mode
        prefs.putBool("configured", false);
        prefs.end();
    }

    WiFiConfig ConfigManager::loadWiFiConfig() {
        WiFiConfig config;

#ifdef ARDUINO
        prefs.begin(NAMESPACE, true); // Read-only mode

        config.configured = prefs.getBool("configured", false);
        config.connection_failures = prefs.getUChar("wifi_failures", 0);

        if (config.configured) {
            prefs.getString("wifi_ssid", config.ssid.data(), config.ssid.size());
            prefs.getString("wifi_pass", config.password.data(), config.password.size());
        }

        prefs.end();
#endif

        return config;
    }

    void ConfigManager::saveWiFiConfig(const WiFiConfig& config) {
#ifdef ARDUINO
        prefs.begin(NAMESPACE, false); // Read-write mode

        prefs.putString("wifi_ssid", config.ssid.data());
        prefs.putString("wifi_pass", config.password.data());
        prefs.putBool("configured", config.configured);

        prefs.end();
#endif
    }

    ShowConfig ConfigManager::loadShowConfig() {
        ShowConfig config;

#ifdef ARDUINO
        prefs.begin(NAMESPACE, true); // Read-only mode

        prefs.getString("show_name", config.current_show.data(), config.current_show.size());

        // If no show name is stored, default constructor already set "Rainbow"
        if (config.current_show[0] == '\0') {
            strcpy(config.current_show.data(), "Rainbow");
        }

        prefs.getString("show_params", config.params_json.data(), config.params_json.size());

        // If no params stored, default constructor already set "{}"
        if (config.params_json[0] == '\0') {
            strcpy(config.params_json.data(), "{}");
        }

        prefs.end();
#endif

        return config;
    }

    void ConfigManager::saveShowConfig(const ShowConfig& config) {
#ifdef ARDUINO
        prefs.begin(NAMESPACE, false); // Read-write mode

        prefs.putString("show_name", config.current_show.data());
        prefs.putString("show_params", config.params_json.data());

        prefs.end();
#endif
    }

    DeviceConfig ConfigManager::loadDeviceConfig() {
        DeviceConfig config;

#ifdef ARDUINO
        prefs.begin(NAMESPACE, true); // Read-only mode

        config.brightness = prefs.getUChar("brightness", 128);
        config.num_pixels = prefs.getUShort("num_pixels", 1);
        config.led_pin = prefs.getUChar("led_pin", PIN_NEOPIXEL);
        config.cycle_time = prefs.getUShort("cycle_time", 10);
        config.gamma_mode =
            static_cast<GammaMode>(prefs.getUChar("gamma_mode", static_cast<uint8_t>(GammaMode::GAMMA_DEFAULT)));
        prefs.getString("device_name", config.device_name.data(), config.device_name.size());

        prefs.end();

        // Always generate device ID from MAC
        String deviceId = getDeviceId();
        strncpy(config.device_id.data(), deviceId.c_str(), config.device_id.size() - 1);
        config.device_id[config.device_id.size() - 1] = '\0';

        // If no custom name is set, use device ID as name
        if (config.device_name[0] == '\0') {
            strncpy(config.device_name.data(), config.device_id.data(), config.device_name.size() - 1);
            config.device_name[config.device_name.size() - 1] = '\0';
        }
#endif

        return config;
    }

    void ConfigManager::saveDeviceConfig(const DeviceConfig& config) {
#ifdef ARDUINO
        prefs.begin(NAMESPACE, false); // Read-write mode

        prefs.putUChar("brightness", config.brightness);
        prefs.putUShort("num_pixels", config.num_pixels);
        prefs.putUChar("led_pin", config.led_pin);
        prefs.putUShort("cycle_time", config.cycle_time);
        prefs.putUChar("gamma_mode", static_cast<uint8_t>(config.gamma_mode));
        prefs.putString("device_name", config.device_name.data());
        // Note: device_id is derived from MAC, not stored

        prefs.end();
#endif
    }

    void ConfigManager::reset() {
#ifdef ARDUINO
        prefs.begin(NAMESPACE, false); // Read-write mode
        prefs.clear();                 // Clear all keys in this namespace
        prefs.end();
#endif
    }

    String ConfigManager::getDeviceId() const {
#ifdef ARDUINO
        uint64_t mac = ESP.getEfuseMac();
        std::array<uint8_t, 6> mac_bytes;
        memcpy(mac_bytes.data(), &mac, 6);

        std::array<char, 16> id;
        snprintf(id.data(), id.size(), "%02X%02X%02X", mac_bytes[3], mac_bytes[4], mac_bytes[5]);
        return String(id.data());
#else
        return String("ledz-000000");
#endif
    }

    uint8_t ConfigManager::incrementConnectionFailures() {
#ifdef ARDUINO
        prefs.begin(NAMESPACE, false); // Read-write mode
        uint8_t failures = prefs.getUChar("wifi_failures", 0);
        failures++;
        prefs.putUChar("wifi_failures", failures);
        prefs.end();
        return failures;
#else
        return 0;
#endif
    }

    void ConfigManager::resetConnectionFailures() {
#ifdef ARDUINO
        prefs.begin(NAMESPACE, false); // Read-write mode
        prefs.putUChar("wifi_failures", 0);
        prefs.end();
#endif
    }

    uint8_t ConfigManager::getConnectionFailures() {
#ifdef ARDUINO
        prefs.begin(NAMESPACE, true); // Read-only mode
        uint8_t failures = prefs.getUChar("wifi_failures", 0);
        prefs.end();
        return failures;
#else
        return 0;
#endif
    }

    LayoutConfig ConfigManager::loadLayoutConfig() {
        LayoutConfig config;

#ifdef ARDUINO
        prefs.begin(NAMESPACE, true); // Read-only mode
        config.reverse = prefs.getBool("layout_reverse", false);
        config.mirror = prefs.getBool("layout_mirror", false);
        config.dead_leds = prefs.getUShort("layout_dead", 0);
        prefs.end();
#endif

        return config;
    }

    void ConfigManager::saveLayoutConfig(const LayoutConfig& config) {
#ifdef ARDUINO
        prefs.begin(NAMESPACE, false); // Read-write mode
        prefs.putBool("layout_reverse", config.reverse);
        prefs.putBool("layout_mirror", config.mirror);
        prefs.putUShort("layout_dead", config.dead_leds);
        prefs.end();

        ESP_LOGD(TAG, "Saved layout - reverse=%d, mirror=%d, dead_leds=%u", config.reverse, config.mirror,
                 config.dead_leds);
#endif
    }

    PresetsConfig ConfigManager::loadPresetsConfig() {
        PresetsConfig presetsConfig;

#ifdef ARDUINO
        prefs.begin(NAMESPACE, true); // Read-only mode

        std::array<char, 20> key;
        for (uint8_t i = 0; i < PresetsConfig::MAX_PRESETS; i++) {
            snprintf(key.data(), key.size(), "preset_%u_valid", i);
            presetsConfig.presets[i].valid = prefs.getBool(key.data(), false);

            if (presetsConfig.presets[i].valid) {
                snprintf(key.data(), key.size(), "preset_%u_name", i);
                prefs.getString(key.data(), presetsConfig.presets[i].name.data(), presetsConfig.presets[i].name.size());

                snprintf(key.data(), key.size(), "preset_%u_show", i);
                prefs.getString(key.data(), presetsConfig.presets[i].show_name.data(),
                                presetsConfig.presets[i].show_name.size());

                snprintf(key.data(), key.size(), "preset_%u_params", i);
                prefs.getString(key.data(), presetsConfig.presets[i].params_json.data(),
                                presetsConfig.presets[i].params_json.size());

                snprintf(key.data(), key.size(), "preset_%u_rev", i);
                presetsConfig.presets[i].layout_reverse = prefs.getBool(key.data(), false);

                snprintf(key.data(), key.size(), "preset_%u_mir", i);
                presetsConfig.presets[i].layout_mirror = prefs.getBool(key.data(), false);

                snprintf(key.data(), key.size(), "preset_%u_dead", i);
                presetsConfig.presets[i].layout_dead_leds = prefs.getShort(key.data(), 0);
            }
        }

        prefs.end();
#endif

        return presetsConfig;
    }

    bool ConfigManager::savePreset(uint8_t index, const Preset& preset) {
        if (index >= PresetsConfig::MAX_PRESETS) {
            return false;
        }

#ifdef ARDUINO
        prefs.begin(NAMESPACE, false); // Read-write mode

        std::array<char, 20> key;

        snprintf(key.data(), key.size(), "preset_%u_valid", index);
        prefs.putBool(key.data(), preset.valid);

        snprintf(key.data(), key.size(), "preset_%u_name", index);
        prefs.putString(key.data(), preset.name.data());

        snprintf(key.data(), key.size(), "preset_%u_show", index);
        prefs.putString(key.data(), preset.show_name.data());

        snprintf(key.data(), key.size(), "preset_%u_params", index);
        prefs.putString(key.data(), preset.params_json.data());

        snprintf(key.data(), key.size(), "preset_%u_rev", index);
        prefs.putBool(key.data(), preset.layout_reverse);

        snprintf(key.data(), key.size(), "preset_%u_mir", index);
        prefs.putBool(key.data(), preset.layout_mirror);

        snprintf(key.data(), key.size(), "preset_%u_dead", index);
        prefs.putShort(key.data(), preset.layout_dead_leds);

        prefs.end();

        ESP_LOGD(TAG, "Saved preset %u '%s'", index, preset.name.data());
        return true;
#else
        return false;
#endif
    }

    bool ConfigManager::deletePreset(uint8_t index) {
        if (index >= PresetsConfig::MAX_PRESETS) {
            return false;
        }

#ifdef ARDUINO
        prefs.begin(NAMESPACE, false); // Read-write mode

        std::array<char, 20> key;

        // Just mark as invalid - NVS keys remain but won't be loaded
        snprintf(key.data(), key.size(), "preset_%u_valid", index);
        prefs.putBool(key.data(), false);

        prefs.end();

        ESP_LOGD(TAG, "Deleted preset %u", index);
        return true;
#else
        return false;
#endif
    }

    int ConfigManager::findPresetByName(const char* name) {
#ifdef ARDUINO
        prefs.begin(NAMESPACE, true); // Read-only mode

        std::array<char, 20> key;
        std::array<char, 32> storedName;

        for (uint8_t i = 0; i < PresetsConfig::MAX_PRESETS; i++) {
            snprintf(key.data(), key.size(), "preset_%u_valid", i);
            if (prefs.getBool(key.data(), false)) {
                snprintf(key.data(), key.size(), "preset_%u_name", i);
                prefs.getString(key.data(), storedName.data(), storedName.size());
                if (strcmp(storedName.data(), name) == 0) {
                    prefs.end();
                    return i;
                }
            }
        }

        prefs.end();
#endif
        return -1;
    }

    int ConfigManager::getNextPresetSlot() {
#ifdef ARDUINO
        prefs.begin(NAMESPACE, true); // Read-only mode

        std::array<char, 20> key;

        for (uint8_t i = 0; i < PresetsConfig::MAX_PRESETS; i++) {
            snprintf(key.data(), key.size(), "preset_%u_valid", i);
            if (!prefs.getBool(key.data(), false)) {
                prefs.end();
                return i;
            }
        }

        prefs.end();
#endif
        return -1;
    }

    TimersConfig ConfigManager::loadTimersConfig() {
        TimersConfig timersConfig;

#ifdef ARDUINO
        prefs.begin(NAMESPACE, true); // Read-only mode

        // Timezone migration, keyed on key presence rather than a schema
        // version: the old and new keys are disjoint, so the absence of "tz"
        // is an unambiguous "not migrated yet". A legacy whole-hour offset
        // becomes a fixed-offset POSIX string so the device keeps exactly
        // the time it kept before; the user opts into DST by picking a
        // region on the timers page.
        bool migrated = false;
        const bool legacyKeyPresent = prefs.isKey("tz_offset");
        if (prefs.isKey("tz")) {
            prefs.getString("tz", timersConfig.timezone.data(), timersConfig.timezone.size());
        } else {
            const int8_t legacyOffsetHours = prefs.getChar("tz_offset", 0);
            LocalTime::legacyOffsetToPosix(legacyOffsetHours, timersConfig.timezone.data(),
                                           timersConfig.timezone.size());
            migrated = true;
            ESP_LOGI(TAG, "Migrating timezone: legacy offset %d hours -> \"%s\"", legacyOffsetHours,
                     timersConfig.timezone.data());
        }

        std::array<char, 20> key;
        for (uint8_t i = 0; i < TimersConfig::MAX_TIMERS; i++) {
            // "timer_%u_en" replaced "timer_%u_enabled" when the slot count
            // went past ten: "timer_10_enabled" is 16 characters, one over
            // the NVS key limit. Slots written by earlier firmware still
            // carry the long key, which is read as a fallback and removed
            // on the next save.
            snprintf(key.data(), key.size(), "timer_%u_en", i);
            if (prefs.isKey(key.data())) {
                timersConfig.timers[i].enabled = prefs.getBool(key.data(), false);
            } else {
                snprintf(key.data(), key.size(), "timer_%u_enabled", i);
                timersConfig.timers[i].enabled = prefs.getBool(key.data(), false);
            }

            if (timersConfig.timers[i].enabled) {
                snprintf(key.data(), key.size(), "timer_%u_type", i);
                timersConfig.timers[i].type = static_cast<TimerType>(prefs.getUChar(key.data(), 0));

                snprintf(key.data(), key.size(), "timer_%u_action", i);
                timersConfig.timers[i].action = static_cast<TimerAction>(prefs.getUChar(key.data(), 0));

                snprintf(key.data(), key.size(), "timer_%u_preset", i);
                timersConfig.timers[i].preset_index = prefs.getUChar(key.data(), 0);

                snprintf(key.data(), key.size(), "timer_%u_target", i);
                timersConfig.timers[i].target_time = prefs.getULong(key.data(), 0);

                snprintf(key.data(), key.size(), "timer_%u_dur", i);
                timersConfig.timers[i].duration_seconds = prefs.getULong(key.data(), 0);

                snprintf(key.data(), key.size(), "timer_%u_lfd", i);
                timersConfig.timers[i].last_fired_yday = prefs.getUShort(key.data(), SCHEDULE_NEVER_FIRED);

                // Both keys are absent for schedules written by earlier
                // firmware; the defaults reproduce the old behaviour
                // exactly: armed, every day.
                snprintf(key.data(), key.size(), "timer_%u_paused", i);
                timersConfig.timers[i].paused = prefs.getBool(key.data(), false);

                snprintf(key.data(), key.size(), "timer_%u_days", i);
                timersConfig.timers[i].days_mask = prefs.getUChar(key.data(), SCHEDULE_EVERY_DAY);

                // Firmware before the POSIX-timezone change used
                // duration_seconds as a "last triggered epoch minute"
                // marker for schedules, so a migrated entry has a number
                // around 29 million where a duration belongs.
                if (migrated && timersConfig.timers[i].type == TimerType::SCHEDULE) {
                    timersConfig.timers[i].duration_seconds = 0;
                }
            }
        }

        prefs.end();

        // The write-back needs its own read-write session: the read above
        // opened the namespace read-only. Losing power before this point
        // leaves tz_offset in place and simply repeats the migration next
        // boot, with the same result. Losing it between the save and the
        // remove leaves a harmless orphan, which is why the removal is
        // driven by the key's presence rather than by `migrated` — the next
        // boot takes the read path and still clears it.
        if (migrated) {
            saveTimersConfig(timersConfig);
        }
        if (legacyKeyPresent) {
            prefs.begin(NAMESPACE, false);
            prefs.remove("tz_offset");
            prefs.end();
        }
#endif

        return timersConfig;
    }

    void ConfigManager::saveTimersConfig(const TimersConfig& config) {
#ifdef ARDUINO
        prefs.begin(NAMESPACE, false); // Read-write mode

        prefs.putString("tz", config.timezone.data());

        std::array<char, 20> key;
        for (uint8_t i = 0; i < TimersConfig::MAX_TIMERS; i++) {
            snprintf(key.data(), key.size(), "timer_%u_en", i);
            prefs.putBool(key.data(), config.timers[i].enabled);

            // Drop the pre-rename key so a later boot cannot read a stale
            // value from it (see loadTimersConfig).
            snprintf(key.data(), key.size(), "timer_%u_enabled", i);
            if (prefs.isKey(key.data())) {
                prefs.remove(key.data());
            }

            if (config.timers[i].enabled) {
                snprintf(key.data(), key.size(), "timer_%u_type", i);
                prefs.putUChar(key.data(), static_cast<uint8_t>(config.timers[i].type));

                snprintf(key.data(), key.size(), "timer_%u_action", i);
                prefs.putUChar(key.data(), static_cast<uint8_t>(config.timers[i].action));

                snprintf(key.data(), key.size(), "timer_%u_preset", i);
                prefs.putUChar(key.data(), config.timers[i].preset_index);

                snprintf(key.data(), key.size(), "timer_%u_target", i);
                prefs.putULong(key.data(), config.timers[i].target_time);

                snprintf(key.data(), key.size(), "timer_%u_dur", i);
                prefs.putULong(key.data(), config.timers[i].duration_seconds);

                snprintf(key.data(), key.size(), "timer_%u_lfd", i);
                prefs.putUShort(key.data(), config.timers[i].last_fired_yday);

                snprintf(key.data(), key.size(), "timer_%u_paused", i);
                prefs.putBool(key.data(), config.timers[i].paused);

                snprintf(key.data(), key.size(), "timer_%u_days", i);
                prefs.putUChar(key.data(), config.timers[i].days_mask);
            }
        }

        prefs.end();

        ESP_LOGD(TAG, "Saved timers configuration");
#endif
    }

    TouchConfig ConfigManager::loadTouchConfig() {
        TouchConfig touchConfig;

#ifdef ARDUINO
        prefs.begin(NAMESPACE, true); // Read-only mode

        touchConfig.enabled = prefs.getBool("touch_enabled", true);
        touchConfig.threshold = prefs.getUShort("touch_thresh", 45000);

        prefs.end();

        ESP_LOGD(TAG, "TouchConfig: Loaded - enabled=%d, threshold=%u", touchConfig.enabled, touchConfig.threshold);
#endif

        return touchConfig;
    }

    void ConfigManager::saveTouchConfig(const TouchConfig& config) {
#ifdef ARDUINO
        prefs.begin(NAMESPACE, false); // Read-write mode

        prefs.putBool("touch_enabled", config.enabled);
        prefs.putUShort("touch_thresh", config.threshold);

        prefs.end();

        ESP_LOGD(TAG, "TouchConfig: Saved - enabled=%d, threshold=%u", config.enabled, config.threshold);
#endif
    }
} // namespace Config
