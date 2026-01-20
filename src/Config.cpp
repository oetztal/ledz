//
// Created by Claude Code on 03.01.26.
//

#include "Config.h"

#ifdef ARDUINO
#include <esp_system.h>
#endif

namespace Config {
    ConfigManager::ConfigManager() {
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
        prefs.begin(NAMESPACE, false); // Read-only mode
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
            prefs.getString("wifi_ssid", config.ssid, sizeof(config.ssid));
            prefs.getString("wifi_pass", config.password, sizeof(config.password));
        }

        prefs.end();
#endif

        return config;
    }

    void ConfigManager::saveWiFiConfig(const WiFiConfig &config) {
#ifdef ARDUINO
        prefs.begin(NAMESPACE, false); // Read-write mode

        prefs.putString("wifi_ssid", config.ssid);
        prefs.putString("wifi_pass", config.password);
        prefs.putBool("configured", config.configured);

        prefs.end();
#endif
    }

    ShowConfig ConfigManager::loadShowConfig() {
        ShowConfig config;

#ifdef ARDUINO
        prefs.begin(NAMESPACE, true); // Read-only mode

        prefs.getString("show_name", config.current_show, sizeof(config.current_show));

        // If no show name is stored, default constructor already set "Rainbow"
        if (config.current_show[0] == '\0') {
            strcpy(config.current_show, "Rainbow");
        }

        prefs.getString("show_params", config.params_json, sizeof(config.params_json));

        // If no params stored, default constructor already set "{}"
        if (config.params_json[0] == '\0') {
            strcpy(config.params_json, "{}");
        }

        config.auto_cycle = prefs.getBool("auto_cycle", true);
        config.cycle_interval_ms = prefs.getUShort("cycle_ms", 60000);

        prefs.end();
#endif

        return config;
    }

    void ConfigManager::saveShowConfig(const ShowConfig &config) {
#ifdef ARDUINO
        prefs.begin(NAMESPACE, false); // Read-write mode

        prefs.putString("show_name", config.current_show);
        prefs.putString("show_params", config.params_json);
        prefs.putBool("auto_cycle", config.auto_cycle);
        prefs.putUShort("cycle_ms", config.cycle_interval_ms);

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
        prefs.getString("device_name", config.device_name, sizeof(config.device_name));

        prefs.end();

        // Always generate device ID from MAC
        String deviceId = getDeviceId();
        strncpy(config.device_id, deviceId.c_str(), sizeof(config.device_id) - 1);
        config.device_id[sizeof(config.device_id) - 1] = '\0';

        // If no custom name is set, use device ID as name
        if (config.device_name[0] == '\0') {
            strncpy(config.device_name, config.device_id, sizeof(config.device_name) - 1);
            config.device_name[sizeof(config.device_name) - 1] = '\0';
        }
#endif

        return config;
    }

    void ConfigManager::saveDeviceConfig(const DeviceConfig &config) {
#ifdef ARDUINO
        prefs.begin(NAMESPACE, false); // Read-write mode

        prefs.putUChar("brightness", config.brightness);
        prefs.putUShort("num_pixels", config.num_pixels);
        prefs.putUChar("led_pin", config.led_pin);
        prefs.putString("device_name", config.device_name);
        // Note: device_id is derived from MAC, not stored

        prefs.end();
#endif
    }

    void ConfigManager::reset() {
#ifdef ARDUINO
        prefs.begin(NAMESPACE, false); // Read-write mode
        prefs.clear(); // Clear all keys in this namespace
        prefs.end();
#endif
    }

    String ConfigManager::getDeviceId() {
#ifdef ARDUINO
        uint64_t mac = ESP.getEfuseMac();
        uint8_t mac_bytes[6];
        memcpy(mac_bytes, &mac, 6);

        char id[16];
        snprintf(id, sizeof(id), "%02X%02X%02X",
                 mac_bytes[3], mac_bytes[4], mac_bytes[5]);
        return String(id);
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

    void ConfigManager::saveLayoutConfig(const LayoutConfig &config) {
#ifdef ARDUINO
        prefs.begin(NAMESPACE, false); // Read-write mode
        prefs.putBool("layout_reverse", config.reverse);
        prefs.putBool("layout_mirror", config.mirror);
        prefs.putUShort("layout_dead", config.dead_leds);
        prefs.end();

        Serial.printf("Config: Saved layout - reverse=%d, mirror=%d, dead_leds=%u\n",
                      config.reverse, config.mirror, config.dead_leds);
#endif
    }

    PresetsConfig ConfigManager::loadPresetsConfig() {
        PresetsConfig presetsConfig;

#ifdef ARDUINO
        prefs.begin(NAMESPACE, true); // Read-only mode

        char key[20];
        for (uint8_t i = 0; i < PresetsConfig::MAX_PRESETS; i++) {
            snprintf(key, sizeof(key), "preset_%u_valid", i);
            presetsConfig.presets[i].valid = prefs.getBool(key, false);

            if (presetsConfig.presets[i].valid) {
                snprintf(key, sizeof(key), "preset_%u_name", i);
                prefs.getString(key, presetsConfig.presets[i].name, sizeof(presetsConfig.presets[i].name));

                snprintf(key, sizeof(key), "preset_%u_show", i);
                prefs.getString(key, presetsConfig.presets[i].show_name, sizeof(presetsConfig.presets[i].show_name));

                snprintf(key, sizeof(key), "preset_%u_params", i);
                prefs.getString(key, presetsConfig.presets[i].params_json, sizeof(presetsConfig.presets[i].params_json));

                snprintf(key, sizeof(key), "preset_%u_rev", i);
                presetsConfig.presets[i].layout_reverse = prefs.getBool(key, false);

                snprintf(key, sizeof(key), "preset_%u_mir", i);
                presetsConfig.presets[i].layout_mirror = prefs.getBool(key, false);

                snprintf(key, sizeof(key), "preset_%u_dead", i);
                presetsConfig.presets[i].layout_dead_leds = prefs.getShort(key, 0);
            }
        }

        prefs.end();
#endif

        return presetsConfig;
    }

    bool ConfigManager::savePreset(uint8_t index, const Preset &preset) {
        if (index >= PresetsConfig::MAX_PRESETS) {
            return false;
        }

#ifdef ARDUINO
        prefs.begin(NAMESPACE, false); // Read-write mode

        char key[20];

        snprintf(key, sizeof(key), "preset_%u_valid", index);
        prefs.putBool(key, preset.valid);

        snprintf(key, sizeof(key), "preset_%u_name", index);
        prefs.putString(key, preset.name);

        snprintf(key, sizeof(key), "preset_%u_show", index);
        prefs.putString(key, preset.show_name);

        snprintf(key, sizeof(key), "preset_%u_params", index);
        prefs.putString(key, preset.params_json);

        snprintf(key, sizeof(key), "preset_%u_rev", index);
        prefs.putBool(key, preset.layout_reverse);

        snprintf(key, sizeof(key), "preset_%u_mir", index);
        prefs.putBool(key, preset.layout_mirror);

        snprintf(key, sizeof(key), "preset_%u_dead", index);
        prefs.putShort(key, preset.layout_dead_leds);

        prefs.end();

        Serial.printf("Config: Saved preset %u '%s'\n", index, preset.name);
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

        char key[20];

        // Just mark as invalid - NVS keys remain but won't be loaded
        snprintf(key, sizeof(key), "preset_%u_valid", index);
        prefs.putBool(key, false);

        prefs.end();

        Serial.printf("Config: Deleted preset %u\n", index);
        return true;
#else
        return false;
#endif
    }

    int ConfigManager::findPresetByName(const char *name) {
#ifdef ARDUINO
        prefs.begin(NAMESPACE, true); // Read-only mode

        char key[20];
        char storedName[32];

        for (uint8_t i = 0; i < PresetsConfig::MAX_PRESETS; i++) {
            snprintf(key, sizeof(key), "preset_%u_valid", i);
            if (prefs.getBool(key, false)) {
                snprintf(key, sizeof(key), "preset_%u_name", i);
                prefs.getString(key, storedName, sizeof(storedName));
                if (strcmp(storedName, name) == 0) {
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

        char key[20];

        for (uint8_t i = 0; i < PresetsConfig::MAX_PRESETS; i++) {
            snprintf(key, sizeof(key), "preset_%u_valid", i);
            if (!prefs.getBool(key, false)) {
                prefs.end();
                return i;
            }
        }

        prefs.end();
#endif
        return -1;
    }
} // namespace Config
