// WebServer Manager - handles HTTP server and API endpoints
//

#ifndef LEDZ_WEBSERVERMANAGER_H
#define LEDZ_WEBSERVERMANAGER_H

#include <atomic>

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
 * Base WebServer Manager
 * Abstract base class for different webserver modes
 */
class WebServerManager {
public:
    // Pointer to the currently-running WebServerManager. AccessLogger middleware
    // uses this to flip the per-instance hasServedAnyRequestFlag. Only one
    // webserver is alive at a time (AP mode -> STA mode -> restart).
    static WebServerManager *activeInstance;

protected:
#ifdef ARDUINO
    AsyncWebServer server;
    AccessLogger logging;
#endif

    Config::ConfigManager &config;
    Network &network;
    ShowController &showController;

    // Set true the first time any HTTP handler runs. The Network task reads
    // this to satisfy OTA_AUTO_CONFIRM_REQUIRE_REQUEST.
    std::atomic<bool> hasServedAnyRequestFlag{false};

    /**
     * Setup shared routes for assets (CSS, favicon, etc.)
     */
    void setupCommonRoutes();

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
    void handleWiFiConfig(AsyncWebServerRequest *request, JsonVariant &doc);
#endif

    /**
     * Setup routes - implemented by subclasses
     */
    virtual void setupRoutes() = 0;

public:
    /**
     * WebServerManager constructor
     * @param config Configuration manager reference
     * @param network Network manager reference
     * @param showController ShowController reference
     */
    WebServerManager(Config::ConfigManager &config, Network &network, ShowController &showController);

    /**
     * Virtual destructor
     */
    virtual ~WebServerManager() = default;

    /**
     * Start the webserver
     * Calls setupRoutes() then starts server
     */
    void begin();

    /**
     * Stop the webserver
     */
    void end();

    /**
     * True if any HTTP handler has run since boot. Read by the Network task
     * for the auto-confirm policy.
     */
    bool hasServedAnyRequest() const { return hasServedAnyRequestFlag.load(); }

    /**
     * Mark that this webserver has just served a request. Called from
     * AccessLogger middleware so we don't have to wire it into every handler.
     */
    void markServedRequest() { hasServedAnyRequestFlag.store(true); }
};

/**
 * Config WebServer Manager
 * Webserver for AP mode - only serves WiFi configuration pages
 */
class ConfigWebServerManager : public WebServerManager {
protected:
    void setupRoutes() override;

public:
    ConfigWebServerManager(Config::ConfigManager &config, Network &network, ShowController &showController);
};

/**
 * Operational WebServer Manager
 * Webserver for STA mode - serves full LED control interface
 */
class OperationalWebServerManager : public WebServerManager {
protected:
    void setupRoutes() override;

public:
    OperationalWebServerManager(Config::ConfigManager &config, Network &network, ShowController &showController);
};

#endif //LEDZ_WEBSERVERMANAGER_H