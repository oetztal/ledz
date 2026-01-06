//
// Created by Andreas W. on 02.01.26.
//
#include <sstream>

#ifdef ARDUINO
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#endif

#include "Network.h"
#include "Config.h"
#include "DeviceId.h"
#include "WebServerManager.h"

#ifdef ARDUINO
#include <set>
#endif

Network::Network(Config::ConfigManager &config)
    : config(config), mode(NetworkMode::NONE), webServer(nullptr)
#ifdef ARDUINO
      , ntpClient(wifiUdp)
#endif
{
}

String Network::generateHostname() {
#ifdef ARDUINO
    String deviceId = DeviceId::getDeviceId();
    String hostname = "ledz-" + deviceId;
    hostname.toLowerCase();
    return hostname;
#else
    return "ledz";
#endif
}

void Network::setWebServer(std::unique_ptr<WebServerManager> &&server) {
    this->webServer = std::move(server);
}

void Network::startAP() {
#ifdef ARDUINO
    mode = NetworkMode::AP;

    // Get device ID for AP SSID
    String ap_ssid = "ledz " + DeviceId::getDeviceId();

    Serial.print("Starting Access Point: ");
    Serial.println(ap_ssid.c_str());

    // Start open AP (no password)
    WiFi.softAP(ap_ssid.c_str());

    IPAddress ip_address = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(ip_address);

    // Start mDNS responder
    String hostname = generateHostname();

    if (MDNS.begin(hostname.c_str())) {
        Serial.print("mDNS responder started: ");
        Serial.print(hostname);
        Serial.println(".local");

        // Load device config for custom name
        Config::DeviceConfig deviceConfig = config.loadDeviceConfig();
        bool hasCustomName = (deviceConfig.device_name[0] != '\0' &&
                              strcmp(deviceConfig.device_name, deviceConfig.device_id) != 0);

        // Set instance name with custom device name or device ID
        String instanceName;
        if (hasCustomName) {
            instanceName = "ledz " + String(deviceConfig.device_name);
        } else {
            instanceName = "ledz " + String(deviceConfig.device_id);
        }
        MDNS.setInstanceName(instanceName.c_str());
        Serial.print("mDNS instance name: ");
        Serial.println(instanceName);

        // Advertise HTTP service
        MDNS.addService("http", "tcp", 80);
    } else {
        Serial.println("Error starting mDNS responder!");
    }

    // Start captive portal (redirects all DNS to this device)
    captivePortal.begin();

#endif
}

void Network::startSTA(const char *ssid, const char *password) {
#ifdef ARDUINO
    mode = NetworkMode::STA;

    WiFi.begin(ssid, password);

    Serial.print("Connecting to WiFi ...");
    Serial.println(ssid);

    // Wait for connection with timeout
    int attempts = 0;
    const int maxAttempts = 30; // 15 seconds

    while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connected");

        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());

        // Start mDNS responder
        String hostname = generateHostname();

        if (MDNS.begin(hostname.c_str())) {
            Serial.print("mDNS responder started: ");
            Serial.print(hostname);
            Serial.println(".local");

            // Load device config for custom name
            Config::DeviceConfig deviceConfig = config.loadDeviceConfig();
            bool hasCustomName = (deviceConfig.device_name[0] != '\0' &&
                                  strcmp(deviceConfig.device_name, deviceConfig.device_id) != 0);

            // Set instance name with custom device name or device ID
            String instanceName;
            if (hasCustomName) {
                instanceName = "ledz " + String(deviceConfig.device_name);
            } else {
                instanceName = "ledz " + String(deviceConfig.device_id);
            }
            MDNS.setInstanceName(instanceName.c_str());
            Serial.print("mDNS instance name: ");
            Serial.println(instanceName);

            // Advertise HTTP service
            MDNS.addService("http", "tcp", 80);

            Serial.println("You can now access ledz at:");
            Serial.print("  http://");
            Serial.print(hostname);
            Serial.println(".local/");
            Serial.print("  or http://");
            Serial.println(WiFi.localIP());
        } else {
            Serial.println("Error starting mDNS responder!");
        }

        // Start NTP client
        ntpClient.begin();
        ntpClient.update();

        Serial.print("NTP time: ");
        Serial.println(ntpClient.getFormattedTime());
    } else {
        Serial.println("\nConnection failed");
    }
#endif
}

[[noreturn]] void Network::task() {
#ifdef ARDUINO
    Serial.println("Network task started");
    // Check if WiFi is configured
    if (!config.isConfigured()) {
        Serial.println("No WiFi configuration found - starting AP mode");

        // Start Access Point mode
        startAP();

        // Start webserver (if available)
        if (webServer != nullptr) {
            webServer->begin();
        }

        // Wait for configuration
        while (!config.isConfigured()) {
            captivePortal.handleClient(); // Handle DNS requests
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        Serial.println("Configuration received");

        vTaskDelay(5000 / portTICK_PERIOD_MS);

        Serial.println("Stopping captive portal");
        captivePortal.end();

        Serial.println("Restarting");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        ESP.restart();
    }
    Serial.println("Network task configured");

    // Check connection failure count
    uint8_t failures = config.getConnectionFailures();
    Serial.print("Previous connection failures: ");
    Serial.println(failures);

    if (failures >= 3) {
        Serial.println("Too many connection failures - starting AP mode for reconfiguration");

        // Start Access Point mode for reconfiguration
        startAP();

        // Start webserver (if available)
        if (webServer != nullptr) {
            webServer->begin();
        }

        // Stay in AP mode until user reconfigures or manually restarts
        while (true) {
            captivePortal.handleClient(); // Handle DNS requests
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }

    // Load WiFi configuration
    Config::WiFiConfig wifiConfig = config.loadWiFiConfig();

    Serial.println("WiFi configured - starting STA mode");

    // Start Station mode
    startSTA(wifiConfig.ssid, wifiConfig.password);

    // Check if connection succeeded
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Failed to connect - incrementing failure counter");
        uint8_t newFailures = config.incrementConnectionFailures();
        Serial.print("New failure count: ");
        Serial.println(newFailures);

        Serial.println("Restarting...");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        ESP.restart();
    }

    // Connection successful - reset failure counter
    config.resetConnectionFailures();

    // Start webserver (if available)
    if (webServer != nullptr) {
        webServer->begin();
    }

    // Main loop - NTP updates
    auto lastNtpUpdate = ntpClient.getEpochTime();

    while (true) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        // Check WiFi connection
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi disconnected - reconnecting ...");
            WiFi.reconnect();

            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 20) {
                vTaskDelay(500 / portTICK_PERIOD_MS);
                attempts++;
            }

            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("WiFi reconnected");
            }
        }

        // Update NTP every 300 seconds
        if (ntpClient.getEpochTime() - lastNtpUpdate > 300) {
            bool result = ntpClient.update();
            Serial.print("NTP update: ");
            Serial.print(ntpClient.getFormattedTime());
            Serial.print(" - ");
            Serial.println(result ? "success" : "failed");
            lastNtpUpdate = ntpClient.getEpochTime();
        }
    }
#endif
}

void Network::startTask() {
    // Create Network task on Core 0 with high priority (3) to ensure webserver responsiveness
    // Priority 3 is higher than LED task (2) to prevent LED rendering from starving network
    xTaskCreatePinnedToCore(
        taskWrapper, // Task Function
        "Network", // Task Name
        10000, // Stack Size
        this, // Parameters
        3, // Priority (increased from 1 to 3)
        &taskHandle, // Task Handle
        0 // Core Number (0)
    );
}

void Network::taskWrapper(void *pvParameters) {
    Serial.println("Network: taskWrapper()");
    auto *instance = static_cast<Network *>(pvParameters);
    instance->task();
}
