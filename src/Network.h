//
// Created by Andreas W. on 02.01.26.
//

#ifndef UNTITLED_WIFI_H
#define UNTITLED_WIFI_H

#include "Network.h"

#ifdef ARDUINO
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#endif

#include "strip/Strip.h"
#include "CaptivePortal.h"

// Forward declarations
namespace Config {
    class ConfigManager;
}

class WebServerManager;

/**
 * Network operating modes
 */
enum class NetworkMode {
    STA, // Station mode (WiFi client)
    AP, // Access Point mode (for configuration)
    NONE // Network disabled
};

class Network {
private:
    NetworkMode mode;

#ifdef ARDUINO
    WiFiUDP wifiUdp;
    NTPClient ntpClient;
#endif

    Config::ConfigManager &config;
    std::unique_ptr<WebServerManager> webServer;
    CaptivePortal captivePortal;
    TaskHandle_t taskHandle = nullptr;

    /**
     * Generate mDNS hostname from device ID
     * Creates hostname like "ledz-aabbcc" from device ID (removes dash)
     */
    String generateHostname();

    /**
     * Start Access Point mode for configuration
     */
    void startAP();

    /**
     * Start Station mode (WiFi client)
     * @param ssid WiFi network name
     * @param password WiFi password
     */
    void startSTA(const char *ssid, const char *password);

public:
    /**
     * Network constructor
     * @param config Configuration manager reference
     * @param status Status indicator reference
     */
    Network(Config::ConfigManager &config);

    // disable copy constructor
    Network(const Network &) = delete;

    /**
     * Set webserver manager (must be called before task starts)
     * @param server Webserver manager pointer
     */
    void setWebServer(std::unique_ptr<WebServerManager> &&server);

    /**
     * Network task (runs on Core 1)
     */
    [[noreturn]] void task();

    void startTask();

    /**
     * Static trampoline function for FreeRTOS
     */
    static void taskWrapper(void *pvParameters);

    /**
     * Get current network mode
     */
    [[nodiscard]] NetworkMode getMode() const { return mode; }
};


#endif //UNTITLED_WIFI_H
