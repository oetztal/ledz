//
// Created by Claude Code on 03.01.26.
//

#ifndef UNTITLED_CONFIG_H
#define UNTITLED_CONFIG_H

#ifdef ARDUINO
#include <Preferences.h>
#include <Arduino.h>
#endif

namespace Config {

    /**
     * WiFi configuration structure
     */
    struct WiFiConfig {
        char ssid[64];
        char password[64];
        bool configured;

        WiFiConfig() : configured(false) {
            ssid[0] = '\0';
            password[0] = '\0';
        }
    };

    /**
     * LED show configuration structure
     */
    struct ShowConfig {
        char current_show[32];      // e.g., "Rainbow", "Mandelbrot"
        char params_json[256];      // JSON string for show parameters
        bool auto_cycle;
        uint16_t cycle_interval_ms; // milliseconds between shows

        ShowConfig() : auto_cycle(true), cycle_interval_ms(60000) {
            strcpy(current_show, "Rainbow");
            strcpy(params_json, "{}");
        }
    };

    /**
     * Device configuration structure
     */
    struct DeviceConfig {
        uint8_t brightness;         // 0-255
        uint16_t num_pixels;
        char device_id[16];         // e.g., "LEDCtrl-AABBCC"

        DeviceConfig() : brightness(128), num_pixels(300) {
            device_id[0] = '\0';
        }
    };

    /**
     * Configuration Manager
     * Handles persistent storage using ESP32 Preferences (NVS)
     */
    class ConfigManager {
    private:
#ifdef ARDUINO
        Preferences prefs;
#endif
        static constexpr const char* NAMESPACE = "ledctrl";

    public:
        ConfigManager();

        /**
         * Initialize the configuration manager
         * Must be called before using other methods
         */
        void begin();

        /**
         * Check if WiFi has been configured
         * @return true if WiFi configuration exists
         */
        bool isConfigured();

        /**
         * Load WiFi configuration from NVS
         * @return WiFiConfig structure
         */
        WiFiConfig loadWiFiConfig();

        /**
         * Save WiFi configuration to NVS
         * @param config WiFi configuration to save
         */
        void saveWiFiConfig(const WiFiConfig& config);

        /**
         * Load show configuration from NVS
         * @return ShowConfig structure with defaults if not found
         */
        ShowConfig loadShowConfig();

        /**
         * Save show configuration to NVS
         * @param config Show configuration to save
         */
        void saveShowConfig(const ShowConfig& config);

        /**
         * Load device configuration from NVS
         * @return DeviceConfig structure with defaults if not found
         */
        DeviceConfig loadDeviceConfig();

        /**
         * Save device configuration to NVS
         * @param config Device configuration to save
         */
        void saveDeviceConfig(const DeviceConfig& config);

        /**
         * Factory reset - clear all stored configuration
         */
        void reset();

        /**
         * Get unique device ID based on MAC address
         * @return Device ID string (e.g., "LEDCtrl-AABBCC")
         */
        String getDeviceId();
    };

} // namespace Config

#endif //UNTITLED_CONFIG_H
