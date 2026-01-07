//
// Created by Claude Code on 03.01.26.
//
// WebServer Manager - handles HTTP server and API endpoints
//

#ifndef UNTITLED_WEBSERVERMANAGER_H
#define UNTITLED_WEBSERVERMANAGER_H

#ifdef ARDUINO
#include <ESPAsyncWebServer.h>
#include <Arduino.h>
#endif

// Forward declarations
namespace Config {
    class ConfigManager;
}

class Network;
class ShowController;
class ShowFactory;

class AccessLogger : public AsyncMiddleware {
public:
    void run(__unused AsyncWebServerRequest* request, __unused ArMiddlewareNext next) override;
};

/**
 * WebServer Manager
 * Manages HTTP server and API endpoints for WiFi configuration
 * and LED show control
 */
class WebServerManager {
private:
#ifdef ARDUINO
    AsyncWebServer server;
    AccessLogger logging;
#endif

    Config::ConfigManager &config;
    Network &network;
    ShowController &showController;

    /**
     * Setup WiFi configuration routes
     */
    void setupConfigRoutes();

    /**
     * Setup API routes for show control
     */
    void setupAPIRoutes();

    /**
     * Handle WiFi configuration POST request
     */
#ifdef ARDUINO
    void handleWiFiConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
#endif

public:
    /**
     * WebServerManager constructor
     * @param config Configuration manager reference
     * @param network Network manager reference
     * @param showController ShowController reference
     */
    WebServerManager(Config::ConfigManager &config, Network &network, ShowController &showController);

    /**
     * Start the webserver
     */
    void begin();

    /**
     * Stop the webserver
     */
    void end();
};

#endif //UNTITLED_WEBSERVERMANAGER_H