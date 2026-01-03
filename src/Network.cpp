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

Network::Network(Config::ConfigManager &config, Status::Status &status)
    : config(config), status(status), mode(NetworkMode::NONE), webServer(nullptr)
#ifdef ARDUINO
    , ntpClient(wifiUdp)
#endif
{
}

void Network::setWebServer(WebServerManager *server) {
    this->webServer = server;
}

void Network::startAP() {
#ifdef ARDUINO
    mode = NetworkMode::AP;

    // Get device ID for AP SSID
    String deviceId = DeviceId::getDeviceId();

    Serial.print("Starting Access Point: ");
    Serial.println(deviceId.c_str());

    // Start open AP (no password)
    WiFi.softAP(deviceId.c_str());

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // Start captive portal (redirects all DNS to this device)
    captivePortal.begin();

    status.connecting(); // Use connecting status for AP mode
#endif
}

void Network::startSTA(const char* ssid, const char* password) {
#ifdef ARDUINO
    mode = NetworkMode::STA;
    status.connecting();

    WiFi.begin(ssid, password);

    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);

    // Wait for connection with timeout
    int attempts = 0;
    const int maxAttempts = 30; // 15 seconds

    while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        status.connected();
        Serial.println("\nConnected!");

        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());

        // Start NTP client
        ntpClient.begin();
        ntpClient.update();

        Serial.print("NTP time: ");
        Serial.println(ntpClient.getFormattedTime());
    } else {
        Serial.println("\nConnection failed!");
        status.connecting(); // Keep in connecting state
    }
#endif
}

[[noreturn]] void Network::task(void *pvParameters) {
#ifdef ARDUINO
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

        Serial.println("Configuration received - stopping captive portal...");
        captivePortal.end();

        Serial.println("Restarting...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        ESP.restart();
    }

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
    Serial.println("Connected successfully - resetting failure counter");
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
            Serial.println("WiFi disconnected - reconnecting...");
            status.connecting();
            WiFi.reconnect();

            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 20) {
                vTaskDelay(500 / portTICK_PERIOD_MS);
                attempts++;
            }

            if (WiFi.status() == WL_CONNECTED) {
                status.connected();
                Serial.println("Reconnected!");
            }
        }

        // Update NTP every 60 seconds
        if (ntpClient.getEpochTime() - lastNtpUpdate > 60) {
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
