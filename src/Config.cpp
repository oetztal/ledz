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

    void ConfigManager::saveWiFiConfig(const WiFiConfig& config) {
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

    void ConfigManager::saveShowConfig(const ShowConfig& config) {
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
        config.num_pixels = prefs.getUShort("num_pixels", 300);

        prefs.end();

        // Always generate device ID from MAC
        String deviceId = getDeviceId();
        strncpy(config.device_id, deviceId.c_str(), sizeof(config.device_id) - 1);
        config.device_id[sizeof(config.device_id) - 1] = '\0';
#endif

        return config;
    }

    void ConfigManager::saveDeviceConfig(const DeviceConfig& config) {
#ifdef ARDUINO
        prefs.begin(NAMESPACE, false); // Read-write mode

        prefs.putUChar("brightness", config.brightness);
        prefs.putUShort("num_pixels", config.num_pixels);
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
        snprintf(id, sizeof(id), "LEDCtrl-%02X%02X%02X",
                 mac_bytes[3], mac_bytes[4], mac_bytes[5]);
        return String(id);
#else
        return String("LEDCtrl-000000");
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

} // namespace Config
