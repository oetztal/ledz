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
        uint8_t connection_failures;  // Track consecutive connection failures

        WiFiConfig() : configured(false), connection_failures(0) {
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
        char device_id[16];         // e.g., "ledz-AABBCC"
        char device_name[32];       // Custom device name

        DeviceConfig() : brightness(128), num_pixels(300) {
            device_id[0] = '\0';
            device_name[0] = '\0';
        }
    };

    /**
     * LED strip layout configuration structure
     */
    struct LayoutConfig {
        bool reverse;               // Reverse LED order
        bool mirror;                // Mirror LED pattern
        uint16_t dead_leds;         // Number of dead LEDs at the end

        LayoutConfig() : reverse(false), mirror(false), dead_leds(0) {
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

        /**
         * Increment WiFi connection failure counter
         * @return New failure count
         */
        uint8_t incrementConnectionFailures();

        /**
         * Reset WiFi connection failure counter
         */
        void resetConnectionFailures();

        /**
         * Get WiFi connection failure count
         * @return Number of consecutive failures
         */
        uint8_t getConnectionFailures();

        /**
         * Load layout configuration from NVS
         * @return LayoutConfig structure with defaults if not found
         */
        LayoutConfig loadLayoutConfig();

        /**
         * Save layout configuration to NVS
         * @param config Layout configuration to save
         */
        void saveLayoutConfig(const LayoutConfig& config);
    };

} // namespace Config

#endif //UNTITLED_CONFIG_H
