#include <sstream>

#ifdef ARDUINO
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <sys/time.h>
#include <lwip/dns.h>  // dns_setserver() — install fallback resolvers
#endif

#include "Network.h"
#include "Config.h"
#include "DeviceId.h"
#include "WebServerManager.h"
#include "OTAUpdater.h"
#include "OTAConfig.h"

#ifdef ARDUINO
#include <set>
#endif

Network::Network(Config::ConfigManager &config, ShowController &showController)
    : config(config), showController(showController), mode(NetworkMode::NONE), webServer(nullptr)
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

    // Disconnect any previous connection
    WiFi.disconnect(true);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Set WiFi mode explicitly
    WiFi.mode(WIFI_STA);

    // Enable modem sleep to reduce power consumption.
    // The WiFi modem sleeps between DTIM beacon intervals and wakes automatically
    // for incoming packets, so the webserver remains fully responsive.
    WiFi.setSleep(WIFI_PS_MIN_MODEM);

    // Use a moderate TX power sufficient for typical indoor distances (< 30 m).
    // Reducing from the 19.5 dBm maximum saves significant transmitter power.
    WiFi.setTxPower(WIFI_POWER_13dBm);

    Serial.println("WiFi Configuration:");
    Serial.printf("  Power save: MIN_MODEM\n");
    Serial.printf("  TX Power: 13dBm\n");

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

        // Print detailed WiFi diagnostics
        Serial.println("\nWiFi Diagnostics:");
        Serial.printf("  SSID: %s\n", WiFi.SSID().c_str());
        Serial.printf("  BSSID: %s\n", WiFi.BSSIDstr().c_str());
        Serial.printf("  Channel: %d\n", WiFi.channel());
        Serial.printf("  RSSI: %d dBm\n", WiFi.RSSI());
        Serial.printf("  MAC: %s\n", WiFi.macAddress().c_str());
        Serial.printf("  Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
        Serial.printf("  DNS: %s\n", WiFi.dnsIP().toString().c_str());
        Serial.printf("  TX Power: %d\n", WiFi.getTxPower());
        Serial.printf("  Sleep Mode: %d (1=MIN_MODEM)\n", WiFi.getSleep());
        Serial.printf("  Auto Reconnect: %d\n", WiFi.getAutoReconnect());
        Serial.println();

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

        // Install public resolvers in lwIP's spare server slots, keeping the
        // DHCP-provided router in slot 0. lwIP moves to the next configured
        // server after DNS_MAX_RETRIES timeouts against the current one, so a
        // router that answers other hosts but silently drops this device's
        // queries no longer breaks every outbound name lookup (OTA included).
        // Slots 1+ are untouched by DHCP, which only ever writes slot 0 here.
        for (uint8_t slot = 1; slot <= 2; ++slot) {
            const char *addr = (slot == 1) ? NET_FALLBACK_DNS_1 : NET_FALLBACK_DNS_2;
            ip_addr_t fallback;
            if (!ipaddr_aton(addr, &fallback)) continue;
            const ip_addr_t *existing = dns_getserver(slot);
            if (existing && !ip_addr_isany(existing)) continue;  // respect DHCP-supplied
            dns_setserver(slot, &fallback);
        }
        Serial.printf("  Resolvers: %s, %s, %s\n",
                      WiFi.dnsIP(0).toString().c_str(),
                      WiFi.dnsIP(1).toString().c_str(),
                      WiFi.dnsIP(2).toString().c_str());

        // Start NTP client
        ntpClient.begin();
        ntpClient.update();

        // Initialize the ESP-IDF SNTP client as well. Unlike the NTPClient
        // polling wrapper, lwip's SNTP actually calls settimeofday() under
        // the hood — which is what mbedtls (used by the OTA TLS handshake)
        // reads through time()/gettimeofday(). Without this, GitHub's leaf
        // certs — valid from 2024+ — look "not yet valid" against the boot
        // RTC (≈ Jan 1970) and the OTA check fails with ESP_ERR_HTTP_CONNECT.
        // Multiple pool servers give lwip a chance to keep working even when
        // one DNS round-trip or UDP-port-123 detour is flaky.
        configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.cloudflare.com");

        Serial.print("NTP time: ");
        Serial.println(ntpClient.getFormattedTime());

        // Initialize timer scheduler
        timerScheduler = std::make_unique<TimerScheduler>(config, showController);
        timerScheduler->begin();
        timerScheduler->setNtpAvailable(true);
    } else {
        Serial.println("\nConnection failed");
    }
#endif
}

void Network::configureUsingAPMode() {
    // Start Access Point mode
    startAP();

    // Create and start config webserver for AP mode
    webServer = std::make_unique<ConfigWebServerManager>(config, *this, showController);
    webServer->begin();

    // Wait for configuration
    while (!config.isConfigured()) {
        captivePortal.handleClient(); // Handle DNS requests
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    Serial.println("Configuration received");

    vTaskDelay(5000 / portTICK_PERIOD_MS);

    Serial.println("Stopping captive portal");
    captivePortal.end();

    Serial.println("Scheduling restart");
    config.requestRestart(1000);

    // Stay in loop until main loop restarts us
    while (true) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

[[noreturn]] void Network::task() {
#ifdef ARDUINO
    Serial.println("Network task started");

    // Initialize touch controller early (works without WiFi)
    touchController = std::make_unique<TouchController>(config, showController);
    touchController->begin();

    // Check if WiFi is configured
    if (!config.isConfigured()) {
        Serial.println("No WiFi configuration found - starting AP mode");

        configureUsingAPMode();
    }
    Serial.println("Network task configured");

    // Check connection failure count
    uint8_t failures = config.getConnectionFailures();
    Serial.print("Previous connection failures: ");
    Serial.println(failures);

    if (failures >= 3) {
        Serial.println("Too many connection failures - starting AP mode for reconfiguration");
        config.markUnconfigured();
        config.resetConnectionFailures();
        configureUsingAPMode();
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

    // Create and start operational webserver for STA mode
    webServer = std::make_unique<OperationalWebServerManager>(config, *this, showController);
    webServer->begin();

    Serial.println("Webserver started - system ready");
    Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());

    // Main loop - NTP updates and touch control
    auto lastNtpUpdate = ntpClient.getEpochTime();

    const unsigned long bootTimeMs = millis();

    unsigned long lastCheck = millis();
    unsigned long lastSecond = millis();
    while (true) {
        // Short delay for responsive touch control
        vTaskDelay(50 / portTICK_PERIOD_MS);

        unsigned long now = millis();

        // Check touch controller frequently (every 50ms)
        if (touchController) {
            touchController->update();
        }

        // Check WiFi and timers every second
        if (now - lastSecond >= 1000) {
            lastSecond = now;

            auto wl_status = WiFi.status();
            if (now - lastCheck > 1100) {
                Serial.printf("WiFi status: %d delayed %lu\n", wl_status, now - lastCheck);
            }
            lastCheck = now;

            // Check WiFi connection
            if (wl_status != WL_CONNECTED) {
                Serial.println("WiFi disconnected - reconnecting ...");
                WiFi.reconnect();

                int attempts = 0;
                while (WiFi.status() != WL_CONNECTED && attempts < 20) {
                    vTaskDelay(500 / portTICK_PERIOD_MS);
                    attempts++;
                }

                if (WiFi.status() == WL_CONNECTED) {
                    Serial.printf("WiFi reconnected (%d attempts)\n", attempts);
                }
            }

            if (ntpClient.getEpochTime() - lastNtpUpdate > 600) {
                bool result = ntpClient.update();
                Serial.print("NTP update: ");
                Serial.print(ntpClient.getFormattedTime());
                Serial.print(" - ");
                Serial.println(result ? "success" : "failed");
                lastNtpUpdate = ntpClient.getEpochTime();
            }

            // Check timers
            if (timerScheduler) {
                timerScheduler->checkTimers(ntpClient.getEpochTime());
            }

            // Auto-confirm an unconfirmed OTA image once the device has been
            // up long enough AND has served at least one HTTP request.
            if (OTAUpdater::hasUnconfirmedUpdate()) {
                bool minUptime = (now - bootTimeMs) >= OTA_AUTO_CONFIRM_MIN_UPTIME_MS;
                bool servedRequest = !OTA_AUTO_CONFIRM_REQUIRE_REQUEST ||
                                     (webServer && webServer->hasServedAnyRequest());
                if (minUptime && servedRequest) {
                    if (OTAUpdater::confirmBoot()) {
                        Serial.printf("Auto-confirmed after %lu ms uptime\n", now - bootTimeMs);
                    }
                }
            }
        }
    }
#endif
}

void Network::startTask() {
    xTaskCreatePinnedToCore(
        taskWrapper, // Task Function
        "Network", // Task Name
        10000, // Stack Size
        this, // Parameters
        1, // Priority
        &taskHandle, // Task Handle
        0 // Core Number
    );
}

void Network::taskWrapper(void *pvParameters) {
    Serial.println("Network: taskWrapper()");
    auto *instance = static_cast<Network *>(pvParameters);
    instance->task();
}
