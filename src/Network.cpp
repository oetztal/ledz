#include <sstream>

#ifdef ARDUINO
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <sys/time.h>
#include <lwip/dns.h>  // dns_setserver() — install fallback resolvers
#endif

#include "Network.h"
#include "Config.h"
#include "Log.h"
#include "DeviceId.h"
#include "WebServerManager.h"
#include "OTAUpdater.h"
#include "OTAConfig.h"

#ifdef ARDUINO
#include <set>
#endif

static const char* TAG = "net";

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

    ESP_LOGI(TAG, "Starting Access Point: %s", ap_ssid.c_str());

    // Start open AP (no password)
    WiFi.softAP(ap_ssid.c_str());

    IPAddress ip_address = WiFi.softAPIP();
    ESP_LOGI(TAG, "AP IP address: %s", ip_address.toString().c_str());

    // Start mDNS responder
    String hostname = generateHostname();

    if (MDNS.begin(hostname.c_str())) {
        ESP_LOGI(TAG, "mDNS responder started: %s.local", hostname.c_str());

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
        ESP_LOGI(TAG, "mDNS instance name: %s", instanceName.c_str());

        // Advertise HTTP service
        MDNS.addService("http", "tcp", 80);
    } else {
        ESP_LOGE(TAG, "Error starting mDNS responder!");
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

    ESP_LOGI(TAG, "WiFi Configuration:");
    ESP_LOGI(TAG, "  Power save: MIN_MODEM");
    ESP_LOGI(TAG, "  TX Power: 13dBm");

    WiFi.begin(ssid, password);

    ESP_LOGI(TAG, "Connecting to WiFi ... %s", ssid);

    // Wait for connection with timeout
    int attempts = 0;
    const int maxAttempts = 30; // 15 seconds

    while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        ESP_LOGI(TAG, "WiFi connected");

        ESP_LOGI(TAG, "IP address: %s", WiFi.localIP().toString().c_str());

        // Print detailed WiFi diagnostics (verbose block, debug-only)
        ESP_LOGD(TAG, "WiFi Diagnostics:");
        ESP_LOGD(TAG, "  SSID: %s", WiFi.SSID().c_str());
        ESP_LOGD(TAG, "  BSSID: %s", WiFi.BSSIDstr().c_str());
        ESP_LOGD(TAG, "  Channel: %d", WiFi.channel());
        ESP_LOGD(TAG, "  RSSI: %d dBm", WiFi.RSSI());
        ESP_LOGD(TAG, "  MAC: %s", WiFi.macAddress().c_str());
        ESP_LOGD(TAG, "  Gateway: %s", WiFi.gatewayIP().toString().c_str());
        ESP_LOGD(TAG, "  DNS: %s", WiFi.dnsIP().toString().c_str());
        ESP_LOGD(TAG, "  TX Power: %d", WiFi.getTxPower());
        ESP_LOGD(TAG, "  Sleep Mode: %d (1=MIN_MODEM)", WiFi.getSleep());
        ESP_LOGD(TAG, "  Auto Reconnect: %d", WiFi.getAutoReconnect());

        // Start mDNS responder
        String hostname = generateHostname();

        if (MDNS.begin(hostname.c_str())) {
            ESP_LOGI(TAG, "mDNS responder started: %s.local", hostname.c_str());

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
            ESP_LOGI(TAG, "mDNS instance name: %s", instanceName.c_str());

            // Advertise HTTP service
            MDNS.addService("http", "tcp", 80);

            ESP_LOGI(TAG, "You can now access ledz at:");
            ESP_LOGI(TAG, "  http://%s.local/", hostname.c_str());
            ESP_LOGI(TAG, "  or http://%s", WiFi.localIP().toString().c_str());
        } else {
            ESP_LOGE(TAG, "Error starting mDNS responder!");
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
        ESP_LOGD(TAG, "  Resolvers: %s, %s, %s",
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
        //
        // The 0,0 offsets also reset TZ to UTC, so this must stay ahead of
        // the scheduler's first TZ apply — checkTimers() does the applying,
        // and it is only reachable from the task loop below, well after this
        // point. A non-UTC TZ is harmless to the handshake regardless:
        // settimeofday/gettimeofday and time() all deal in UTC epoch
        // seconds, and TZ only affects the localtime() family, which mbedtls
        // does not use for certificate validity.
        configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.cloudflare.com");

        ESP_LOGI(TAG, "NTP time: %s", ntpClient.getFormattedTime());

        // Initialize timer scheduler
        timerScheduler = std::make_unique<TimerScheduler>(config, showController);
        timerScheduler->begin();
        timerScheduler->setNtpAvailable(true);
    } else {
        ESP_LOGW(TAG, "Connection failed");
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
    ESP_LOGI(TAG, "Configuration received");

    vTaskDelay(5000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Stopping captive portal");
    captivePortal.end();

    ESP_LOGI(TAG, "Scheduling restart");
    config.requestRestart(1000);

    // Stay in loop until main loop restarts us
    while (true) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

[[noreturn]] void Network::task() {
#ifdef ARDUINO
    ESP_LOGI(TAG, "Network task started");

    // Initialize touch controller early (works without WiFi)
    touchController = std::make_unique<TouchController>(config, showController);
    touchController->begin();

    // Check if WiFi is configured
    if (!config.isConfigured()) {
        ESP_LOGW(TAG, "No WiFi configuration found - starting AP mode");

        configureUsingAPMode();
    }
    ESP_LOGI(TAG, "Network task configured");

    // Check connection failure count
    uint8_t failures = config.getConnectionFailures();
    ESP_LOGI(TAG, "Previous connection failures: %u", failures);

    if (failures >= 3) {
        ESP_LOGW(TAG, "Too many connection failures - starting AP mode for reconfiguration");
        config.markUnconfigured();
        config.resetConnectionFailures();
        configureUsingAPMode();
    }

    // Load WiFi configuration
    Config::WiFiConfig wifiConfig = config.loadWiFiConfig();

    ESP_LOGI(TAG, "WiFi configured - starting STA mode");

    // Start Station mode
    startSTA(wifiConfig.ssid, wifiConfig.password);

    // Check if connection succeeded
    if (WiFi.status() != WL_CONNECTED) {
        ESP_LOGW(TAG, "Failed to connect - incrementing failure counter");
        uint8_t newFailures = config.incrementConnectionFailures();
        ESP_LOGI(TAG, "New failure count: %u", newFailures);

        ESP_LOGI(TAG, "Restarting...");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        ESP.restart();
    }

    // Connection successful - reset failure counter
    config.resetConnectionFailures();

    // Create and start operational webserver for STA mode
    webServer = std::make_unique<OperationalWebServerManager>(config, *this, showController);
    webServer->begin();

    ESP_LOGI(TAG, "Webserver started - system ready");
    ESP_LOGD(TAG, "Free heap: %u bytes", ESP.getFreeHeap());

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
                ESP_LOGD(TAG, "WiFi status: %d delayed %lu", wl_status, now - lastCheck);
            }
            lastCheck = now;

            // Check WiFi connection
            if (wl_status != WL_CONNECTED) {
                ESP_LOGW(TAG, "WiFi disconnected - reconnecting ...");
                WiFi.reconnect();

                int attempts = 0;
                while (WiFi.status() != WL_CONNECTED && attempts < 20) {
                    vTaskDelay(500 / portTICK_PERIOD_MS);
                    attempts++;
                }

                if (WiFi.status() == WL_CONNECTED) {
                    ESP_LOGI(TAG, "WiFi reconnected (%d attempts)", attempts);
                }
            }

            if (ntpClient.getEpochTime() - lastNtpUpdate > 600) {
                bool result = ntpClient.update();
                ESP_LOGD(TAG, "NTP update: %s - %s",
                               ntpClient.getFormattedTime(),
                               result ? "success" : "failed");
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
                        ESP_LOGI(TAG, "Auto-confirmed after %lu ms uptime", now - bootTimeMs);
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
    ESP_LOGI(TAG, "taskWrapper()");
    auto *instance = static_cast<Network *>(pvParameters);
    instance->task();
}
