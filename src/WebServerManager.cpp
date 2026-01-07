//
// Created by Claude Code on 03.01.26.
//

#include "WebServerManager.h"

#include <sstream>

#include "Config.h"
#include "Network.h"
#include "ShowController.h"
#include "ShowFactory.h"
#include "DeviceId.h"
#include "OTAUpdater.h"
#include "OTAConfig.h"

#ifdef ARDUINO
#include <ArduinoJson.h>
#include <esp_ota_ops.h>
// Include compressed web content
#include "generated/config_gz.h"
#include "generated/control_gz.h"
#endif

// Helper function to send gzipped response
#ifdef ARDUINO
static void sendGzippedHtml(AsyncWebServerRequest *request, const uint8_t *data, size_t len) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", data, len);
    response->addHeader("Content-Encoding", "gzip");
    response->addHeader("Cache-Control", "max-age=86400");
    request->send(response);
}
#endif

// HTML source files are in data/ directory
// Run: python3 scripts/compress_web.py to regenerate compressed headers
// Compressed content: CONFIG_GZ, CONTROL_GZ (87% size reduction)

WebServerManager::WebServerManager(Config::ConfigManager &config, Network &network, ShowController &show_controller)
    : config(config), network(network), showController(show_controller)
#ifdef ARDUINO
      , server(80)
#endif
{
}

void AccessLogger::run(AsyncWebServerRequest *request, ArMiddlewareNext next) {
    Print *_out = &Serial;
    std::stringstream ss;
    ss << "[HTTP] " << request->client()->remoteIP().toString().c_str() << " " << request->url().c_str() << " " <<
            request->methodToString();

    uint32_t elapsed = millis();
    next();
    elapsed = millis() - elapsed;

    AsyncWebServerResponse *response = request->getResponse();
    if (response) {
        ss << " (" << elapsed << " ms) " << response->code();
    } else {
        ss << " (no response)";
    }
    _out->println(ss.str().c_str());
}

void WebServerManager::setupConfigRoutes() {
#ifdef ARDUINO
    Serial.println("Setting up config routes...");

    // Serve WiFi config page (gzip compressed)
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        sendGzippedHtml(request, CONFIG_GZ, CONFIG_GZ_LEN);
    });

    // Handle WiFi configuration POST
    server.on("/api/wifi", HTTP_POST,
              [](AsyncWebServerRequest *request) {
                  // This callback is called after body processing
              },
              NULL,
              [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                  this->handleWiFiConfig(request, data, len, index, total);
              }
    );
#endif
}

void WebServerManager::setupAPIRoutes() {
#ifdef ARDUINO
    Serial.println("Setting up API routes...");

    // Serve main control page (gzip compressed)
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        sendGzippedHtml(request, CONTROL_GZ, CONTROL_GZ_LEN);
    });

    // GET /api/status - Get device status
    server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        StaticJsonDocument<1024> doc;

        // Device info
        Config::DeviceConfig deviceConfig = config.loadDeviceConfig();
        doc["device_id"] = deviceConfig.device_id;
        doc["device_name"] = deviceConfig.device_name;
        doc["brightness"] = showController.getBrightness();
        doc["firmware_version"] = FIRMWARE_VERSION;

        // OTA partition info
        const esp_partition_t *running_partition = esp_ota_get_running_partition();
        if (running_partition != nullptr) {
            doc["ota_partition"] = running_partition->label;
        }

        // Show info
        doc["current_show"] = showController.getCurrentShowName();
        doc["auto_cycle"] = showController.isAutoCycleEnabled();

        // Current show configuration
        Config::ShowConfig showConfig = config.loadShowConfig();
        if (strlen(showConfig.params_json) > 0) {
            // Parse the params_json and include it
            StaticJsonDocument<512> paramsDoc;
            DeserializationError error = deserializeJson(paramsDoc, showConfig.params_json);
            if (!error) {
                doc["show_params"] = paramsDoc.as<JsonObject>();
            }
        }

        // Network info
        doc["wifi_connected"] = (WiFi.status() == WL_CONNECTED);
        if (WiFi.status() == WL_CONNECTED) {
            doc["ip_address"] = WiFi.localIP().toString();
            doc["wifi_ssid"] = WiFi.SSID();
        }

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // GET /api/shows - List available shows
    server.on("/api/shows", HTTP_GET, [this](AsyncWebServerRequest *request) {
        StaticJsonDocument<1024> doc;
        JsonArray shows = doc.createNestedArray("shows");

        const std::vector<ShowFactory::ShowInfo> &showList = showController.listShows();
        for (const auto &showInfo: showList) {
            JsonObject show = shows.createNestedObject();
            show["name"] = showInfo.name;
            show["description"] = showInfo.description;
        }

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // POST /api/show - Change current show
    server.on("/api/show", HTTP_POST,
              [](AsyncWebServerRequest *request) {
              },
              NULL,
              [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                  if (index == 0) {
                      StaticJsonDocument<512> doc;
                      DeserializationError error = deserializeJson(doc, data, len);

                      if (error) {
                          request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
                          return;
                      }

                      const char *showName = doc["name"];
                      if (showName == nullptr) {
                          request->send(400, "application/json",
                                        "{\"success\":false,\"error\":\"Show name required\"}");
                          return;
                      }

                      // Get parameters if provided
                      String paramsJson;
                      if (doc.containsKey("params")) {
                          JsonObject params = doc["params"];
                          serializeJson(params, paramsJson);
                      } else {
                          paramsJson = "{}";
                      }

                      if (showController.queueShowChange(showName, paramsJson.c_str())) {
                          request->send(200, "application/json", "{\"success\":true}");
                      } else {
                          request->send(503, "application/json", "{\"success\":false,\"error\":\"Queue full\"}");
                      }
                  }
              }
    );

    // POST /api/brightness - Change brightness
    server.on("/api/brightness", HTTP_POST,
              [](AsyncWebServerRequest *request) {
              },
              NULL,
              [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                  if (index == 0) {
                      StaticJsonDocument<256> doc;
                      DeserializationError error = deserializeJson(doc, data, len);

                      if (error) {
                          request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
                          return;
                      }

                      if (!doc.containsKey("value")) {
                          request->send(400, "application/json",
                                        "{\"success\":false,\"error\":\"Brightness value required\"}");
                          return;
                      }

                      uint8_t brightness = doc["value"];
                      if (showController.queueBrightnessChange(brightness)) {
                          request->send(200, "application/json", "{\"success\":true}");
                      } else {
                          request->send(503, "application/json", "{\"success\":false,\"error\":\"Queue full\"}");
                      }
                  }
              }
    );

    // POST /api/auto-cycle - Toggle auto-cycle
    server.on("/api/auto-cycle", HTTP_POST,
              [](AsyncWebServerRequest *request) {
              },
              NULL,
              [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                  if (index == 0) {
                      StaticJsonDocument<256> doc;
                      DeserializationError error = deserializeJson(doc, data, len);

                      if (error) {
                          request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
                          return;
                      }

                      if (!doc.containsKey("enabled")) {
                          request->send(400, "application/json",
                                        "{\"success\":false,\"error\":\"Enabled field required\"}");
                          return;
                      }

                      bool enabled = doc["enabled"];
                      if (showController.queueAutoCycleToggle(enabled)) {
                          request->send(200, "application/json", "{\"success\":true}");
                      } else {
                          request->send(503, "application/json", "{\"success\":false,\"error\":\"Queue full\"}");
                      }
                  }
              }
    );

    // POST /api/layout - Change strip layout configuration
    server.on("/api/layout", HTTP_POST,
              [](AsyncWebServerRequest *request) {
              },
              NULL,
              [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                  if (index == 0) {
                      StaticJsonDocument<256> doc;
                      DeserializationError error = deserializeJson(doc, data, len);

                      if (error) {
                          request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
                          return;
                      }

                      Config::LayoutConfig layoutConfig = config.loadLayoutConfig();

                      // Update fields if provided
                      if (doc.containsKey("reverse")) {
                          layoutConfig.reverse = doc["reverse"];
                      }
                      if (doc.containsKey("mirror")) {
                          layoutConfig.mirror = doc["mirror"];
                      }
                      if (doc.containsKey("dead_leds")) {
                          layoutConfig.dead_leds = doc["dead_leds"];
                      }

                      // Queue the layout change for thread-safe execution
                      if (showController.queueLayoutChange(layoutConfig.reverse, layoutConfig.mirror,
                                                           layoutConfig.dead_leds)) {
                          request->send(200, "application/json", "{\"success\":true}");
                      } else {
                          request->send(503, "application/json", "{\"success\":false,\"error\":\"Queue full\"}");
                      }
                  }
              }
    );

    // GET /api/layout - Get current layout configuration
    server.on("/api/layout", HTTP_GET, [this](AsyncWebServerRequest *request) {
        Config::LayoutConfig layoutConfig = config.loadLayoutConfig();

        StaticJsonDocument<256> doc;
        doc["reverse"] = layoutConfig.reverse;
        doc["mirror"] = layoutConfig.mirror;
        doc["dead_leds"] = layoutConfig.dead_leds;

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // POST /api/restart - Restart the device
    server.on("/api/restart", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Restarting...\"}");
        delay(500); // Give time for response to send
        ESP.restart();
    });

    // POST /api/settings/wifi - Update WiFi credentials
    server.on("/api/settings/wifi", HTTP_POST,
              [](AsyncWebServerRequest *request) {
              },
              NULL,
              [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                  if (index == 0) {
                      StaticJsonDocument<256> doc;
                      DeserializationError error = deserializeJson(doc, data, len);

                      if (error) {
                          request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
                          return;
                      }

                      const char *ssid = doc["ssid"];
                      const char *password = doc["password"];

                      if (ssid == nullptr || strlen(ssid) == 0) {
                          request->send(400, "application/json", "{\"success\":false,\"error\":\"SSID required\"}");
                          return;
                      }

                      // Create and save WiFi config
                      Config::WiFiConfig wifiConfig;
                      strncpy(wifiConfig.ssid, ssid, sizeof(wifiConfig.ssid) - 1);
                      wifiConfig.ssid[sizeof(wifiConfig.ssid) - 1] = '\0';

                      if (password != nullptr) {
                          strncpy(wifiConfig.password, password, sizeof(wifiConfig.password) - 1);
                          wifiConfig.password[sizeof(wifiConfig.password) - 1] = '\0';
                      } else {
                          wifiConfig.password[0] = '\0';
                      }

                      wifiConfig.configured = true;
                      config.saveWiFiConfig(wifiConfig);

                      Serial.printf("WiFi credentials updated: SSID=%s\n", wifiConfig.ssid);

                      // Send success response and restart
                      request->send(200, "application/json",
                                    "{\"success\":true,\"message\":\"WiFi updated, restarting...\"}");
                      delay(1000); // Give time for response to send
                      ESP.restart();
                  }
              }
    );

    // POST /api/settings/device-name - Update device name
    server.on("/api/settings/device-name", HTTP_POST,
              [](AsyncWebServerRequest *request) {
              },
              NULL,
              [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                  if (index == 0) {
                      StaticJsonDocument<256> doc;
                      DeserializationError error = deserializeJson(doc, data, len);

                      if (error) {
                          request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
                          return;
                      }

                      const char *name = doc["name"];

                      if (name == nullptr || strlen(name) == 0) {
                          request->send(400, "application/json",
                                        "{\"success\":false,\"error\":\"Device name required\"}");
                          return;
                      }

                      // Load current config, update name, and save
                      Config::DeviceConfig deviceConfig = config.loadDeviceConfig();
                      strncpy(deviceConfig.device_name, name, sizeof(deviceConfig.device_name) - 1);
                      deviceConfig.device_name[sizeof(deviceConfig.device_name) - 1] = '\0';
                      config.saveDeviceConfig(deviceConfig);

                      Serial.printf("Device name updated: %s\n", deviceConfig.device_name);

                      // Send success response (no restart needed)
                      request->send(200, "application/json", "{\"success\":true}");
                  }
              }
    );

    // POST /api/settings/device - Update device hardware settings (num_pixels, led_pin)
    server.on("/api/settings/device", HTTP_POST,
              [](AsyncWebServerRequest *request) {
              },
              NULL,
              [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                  if (index == 0) {
                      StaticJsonDocument<256> doc;
                      DeserializationError error = deserializeJson(doc, data, len);

                      if (error) {
                          request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
                          return;
                      }

                      // Load current config
                      Config::DeviceConfig deviceConfig = config.loadDeviceConfig();
                      bool changed = false;

                      // Update num_pixels if provided
                      if (doc.containsKey("num_pixels")) {
                          uint16_t num_pixels = doc["num_pixels"];

                          if (num_pixels < 1 || num_pixels > 1000) {
                              request->send(400, "application/json",
                                            "{\"success\":false,\"error\":\"Number of pixels must be between 1 and 1000\"}");
                              return;
                          }

                          deviceConfig.num_pixels = num_pixels;
                          Serial.printf("Number of pixels updated: %u\n", num_pixels);
                          changed = true;
                      }

                      // Update led_pin if provided
                      if (doc.containsKey("led_pin")) {
                          uint8_t led_pin = doc["led_pin"];

                          if (led_pin > 48) {
                              request->send(400, "application/json",
                                            "{\"success\":false,\"error\":\"LED pin must be between 0 and 48\"}");
                              return;
                          }

                          deviceConfig.led_pin = led_pin;
                          Serial.printf("LED pin updated: %u\n", led_pin);
                          changed = true;
                      }

                      if (!changed) {
                          request->send(400, "application/json",
                                        "{\"success\":false,\"error\":\"No valid parameters provided\"}");
                          return;
                      }

                      // Save config
                      config.saveDeviceConfig(deviceConfig);

                      // Send success response and restart
                      request->send(200, "application/json",
                                    "{\"success\":true,\"message\":\"Device settings updated, restarting...\"}");
                      delay(1000); // Give time for response to send
                      ESP.restart();
                  }
              }
    );

    // POST /api/settings/factory-reset - Factory reset device
    server.on("/api/settings/factory-reset", HTTP_POST, [this](AsyncWebServerRequest *request) {
        Serial.println("Factory reset requested");

        // Send success response first
        request->send(200, "application/json",
                      "{\"success\":true,\"message\":\"Factory reset complete, restarting...\"}");

        // Give time for response to send
        delay(500);

        // Clear all configuration
        config.reset();

        Serial.println("All settings cleared");

        // Restart
        ESP.restart();
    });

    // GET /api/about - Device information
    server.on("/api/about", HTTP_GET, [this](AsyncWebServerRequest *request) {
        StaticJsonDocument<1024> doc;

        // Device info
        Config::DeviceConfig deviceConfig = config.loadDeviceConfig();
        doc["device_id"] = deviceConfig.device_id;
        doc["num_pixels"] = deviceConfig.num_pixels;
        doc["led_pin"] = deviceConfig.led_pin;

        // Chip info
        doc["chip_model"] = ESP.getChipModel();
        doc["chip_revision"] = ESP.getChipRevision();
        doc["chip_cores"] = ESP.getChipCores();
        doc["cpu_freq_mhz"] = ESP.getCpuFreqMHz();

        // Memory info
        doc["free_heap"] = ESP.getFreeHeap();
        doc["heap_size"] = ESP.getHeapSize();
        doc["min_free_heap"] = ESP.getMinFreeHeap();
        doc["psram_size"] = ESP.getPsramSize();

        // Flash info
        doc["flash_size"] = ESP.getFlashChipSize();
        doc["flash_speed"] = ESP.getFlashChipSpeed();

        // Runtime info
        doc["uptime_ms"] = millis();

        // Network info
        if (WiFi.status() == WL_CONNECTED) {
            doc["wifi_ssid"] = WiFi.SSID();
            doc["wifi_rssi"] = WiFi.RSSI();
            doc["ip_address"] = WiFi.localIP().toString();
            doc["mac_address"] = WiFi.macAddress();
        } else if (WiFi.getMode() == WIFI_AP) {
            doc["ap_ssid"] = WiFi.softAPgetHostname();
            doc["ap_ip"] = WiFi.softAPIP().toString();
            doc["ap_clients"] = WiFi.softAPgetStationNum();
        }

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // GET /api/ota/check - Check for firmware updates
    server.on("/api/ota/check", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<1024> doc;

        FirmwareInfo info;
        bool updateAvailable = OTAUpdater::checkForUpdate(OTA_GITHUB_OWNER, OTA_GITHUB_REPO, info);

        if (updateAvailable) {
            doc["update_available"] = true;
            doc["current_version"] = FIRMWARE_VERSION;
            doc["latest_version"] = info.version;
            doc["release_name"] = info.name;
            doc["size_bytes"] = info.size;
            doc["download_url"] = info.downloadUrl;
            doc["release_notes"] = info.releaseNotes;
        } else {
            doc["update_available"] = false;
            doc["current_version"] = FIRMWARE_VERSION;
            doc["message"] = "No update available or failed to check";
        }

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // POST /api/ota/update - Perform OTA update
    server.on("/api/ota/update", HTTP_POST,
              [](AsyncWebServerRequest *request) {
                  // Send immediate response before starting update
                  request->send(200, "application/json",
                                "{\"status\":\"starting\",\"message\":\"OTA update started\"}");
              },
              nullptr,
              [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                  if (index == 0) {
                      Serial.println("[WebServer] OTA update requested");
                  }

                  if (index + len == total) {
                      StaticJsonDocument<512> doc;
                      deserializeJson(doc, data, len);

                      String downloadUrl = doc["download_url"] | "";
                      size_t size = doc["size"] | 0;

                      if (downloadUrl.isEmpty() || size == 0) {
                          return;
                      }

                      Serial.printf("[WebServer] Starting OTA: %s (%zu bytes)\n", downloadUrl.c_str(), size);

                      // Perform OTA update (this will block)
                      bool success = OTAUpdater::performUpdate(downloadUrl, size, [](int percent, size_t bytes) {
                          Serial.printf("[OTA] Progress: %d%% (%zu bytes)\n", percent, bytes);
                      });

                      if (success) {
                          Serial.println("[WebServer] OTA update successful, restarting...");
                          delay(1000);
                          ESP.restart();
                      } else {
                          Serial.println("[WebServer] OTA update failed!");
                      }
                  }
              }
    );

    // GET /api/ota/status - Get OTA status
    server.on("/api/ota/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<512> doc;

        doc["firmware_version"] = FIRMWARE_VERSION;
        doc["build_date"] = FIRMWARE_BUILD_DATE;
        doc["build_time"] = FIRMWARE_BUILD_TIME;

        // Partition info
        String partitionLabel;
        uint32_t partitionAddress;
        if (OTAUpdater::getRunningPartitionInfo(partitionLabel, partitionAddress)) {
            doc["partition"] = partitionLabel;
            doc["partition_address"] = partitionAddress;
        }

        // Check if running unconfirmed update
        doc["unconfirmed_update"] = OTAUpdater::hasUnconfirmedUpdate();

        // Memory info
        uint32_t freeHeap, minFreeHeap, psramFree;
        OTAUpdater::getMemoryInfo(freeHeap, minFreeHeap, psramFree);
        doc["free_heap"] = freeHeap;
        doc["min_free_heap"] = minFreeHeap;
        doc["psram_free"] = psramFree;
        doc["ota_safe"] = OTAUpdater::hasEnoughMemory();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // POST /api/ota/confirm - Confirm successful boot after OTA
    server.on("/api/ota/confirm", HTTP_POST, [](AsyncWebServerRequest *request) {
        bool success = OTAUpdater::confirmBoot();

        StaticJsonDocument<256> doc;
        doc["success"] = success;
        doc["message"] = success ? "Boot confirmed, rollback disabled" : "Failed to confirm boot";

        String response;
        serializeJson(doc, response);
        request->send(success ? 200 : 500, "application/json", response);
    });

    // GET /about - About page
    server.on("/about", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>About - ledz</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        .container {
            max-width: 600px;
            margin: 0 auto;
            background-color: white;
            border-radius: 15px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.2);
            overflow: hidden;
        }
        .header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px;
            text-align: center;
        }
        .header h1 { font-size: 28px; margin-bottom: 10px; }
        .nav {
            padding: 10px 20px;
            background-color: #f8f9fa;
            border-bottom: 1px solid #dee2e6;
        }
        .nav a {
            color: #667eea;
            text-decoration: none;
            font-weight: 600;
        }
        .content { padding: 30px; }
        .info-section {
            margin-bottom: 25px;
            padding-bottom: 20px;
            border-bottom: 1px solid #e9ecef;
        }
        .info-section:last-child { border-bottom: none; }
        .info-section h2 {
            font-size: 18px;
            color: #333;
            margin-bottom: 15px;
        }
        .info-row {
            display: flex;
            justify-content: space-between;
            padding: 8px 0;
        }
        .info-label {
            font-size: 14px;
            color: #6c757d;
        }
        .info-value {
            font-size: 14px;
            font-weight: 600;
            color: #333;
        }
        .refresh-btn {
            width: 100%;
            padding: 12px;
            background-color: #667eea;
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            margin-top: 20px;
        }
        .refresh-btn:hover { background-color: #5568d3; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>Device Information</h1>
        </div>
        <div class="nav">
            <a href="/">&larr; Back to Control</a>
        </div>
        <div class="content">
            <div class="info-section">
                <h2>Device</h2>
                <div id="deviceInfo"></div>
            </div>
            <div class="info-section">
                <h2>Hardware</h2>
                <div id="hardwareInfo"></div>
            </div>
            <div class="info-section">
                <h2>Memory</h2>
                <div id="memoryInfo"></div>
            </div>
            <div class="info-section">
                <h2>Network</h2>
                <div id="networkInfo"></div>
            </div>
            <button class="refresh-btn" onclick="loadInfo()">Refresh</button>
        </div>
    </div>
    <script>
        function formatBytes(bytes) {
            if (bytes < 1024) return bytes + ' B';
            if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
            return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
        }
        function formatUptime(ms) {
            const seconds = Math.floor(ms / 1000);
            const minutes = Math.floor(seconds / 60);
            const hours = Math.floor(minutes / 60);
            const days = Math.floor(hours / 24);
            if (days > 0) return days + 'd ' + (hours % 24) + 'h';
            if (hours > 0) return hours + 'h ' + (minutes % 60) + 'm';
            return minutes + 'm ' + (seconds % 60) + 's';
        }
        function createRow(label, value) {
            return `<div class="info-row"><span class="info-label">${label}</span><span class="info-value">${value}</span></div>`;
        }
        async function loadInfo() {
            try {
                const response = await fetch('/api/about');
                const data = await response.json();

                document.getElementById('deviceInfo').innerHTML =
                    createRow('Device ID', data.device_id) +
                    createRow('Uptime', formatUptime(data.uptime_ms));

                document.getElementById('hardwareInfo').innerHTML =
                    createRow('Chip Model', data.chip_model) +
                    createRow('Chip Revision', data.chip_revision) +
                    createRow('CPU Cores', data.chip_cores) +
                    createRow('CPU Frequency', data.cpu_freq_mhz + ' MHz');

                const heapUsage = ((data.heap_size - data.free_heap) / data.heap_size * 100).toFixed(1);
                document.getElementById('memoryInfo').innerHTML =
                    createRow('Free Heap', formatBytes(data.free_heap)) +
                    createRow('Total Heap', formatBytes(data.heap_size)) +
                    createRow('Heap Usage', heapUsage + '%') +
                    createRow('Min Free Heap', formatBytes(data.min_free_heap)) +
                    createRow('Flash Size', formatBytes(data.flash_size));

                let networkHTML = '';
                if (data.wifi_ssid) {
                    networkHTML =
                        createRow('Status', 'Connected') +
                        createRow('SSID', data.wifi_ssid) +
                        createRow('Signal', data.wifi_rssi + ' dBm') +
                        createRow('IP Address', data.ip_address) +
                        createRow('MAC Address', data.mac_address);
                } else if (data.ap_ssid) {
                    networkHTML =
                        createRow('Status', 'Access Point') +
                        createRow('SSID', data.ap_ssid) +
                        createRow('IP Address', data.ap_ip) +
                        createRow('Clients', data.ap_clients);
                } else {
                    networkHTML = createRow('Status', 'Disconnected');
                }
                document.getElementById('networkInfo').innerHTML = networkHTML;
            } catch (error) {
                console.error('Failed to load device info:', error);
            }
        }
        loadInfo();
    </script>
</body>
</html>
        )rawliteral");
    });

    // GET /settings - Settings page
    server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Settings - ledz</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        .container {
            max-width: 600px;
            margin: 0 auto;
            background-color: white;
            border-radius: 15px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.2);
            overflow: hidden;
        }
        .header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px;
            text-align: center;
        }
        .header h1 { font-size: 28px; margin-bottom: 5px; }
        .header p { font-size: 14px; opacity: 0.9; }
        .nav {
            padding: 10px 20px;
            background-color: #f8f9fa;
            border-bottom: 1px solid #dee2e6;
        }
        .nav a {
            color: #667eea;
            text-decoration: none;
            font-weight: 600;
        }
        .content { padding: 30px; }
        .settings-section {
            margin-bottom: 30px;
            padding-bottom: 25px;
            border-bottom: 1px solid #e9ecef;
        }
        .settings-section:last-child { border-bottom: none; }
        .settings-section h2 {
            font-size: 18px;
            color: #333;
            margin-bottom: 10px;
        }
        .settings-section p {
            font-size: 13px;
            color: #6c757d;
            margin-bottom: 15px;
        }
        .form-group {
            margin-bottom: 15px;
        }
        .form-group label {
            display: block;
            font-size: 14px;
            color: #333;
            margin-bottom: 5px;
            font-weight: 600;
        }
        .form-group input {
            width: 100%;
            padding: 12px;
            border: 2px solid #e9ecef;
            border-radius: 8px;
            font-size: 16px;
        }
        .form-group input:focus {
            outline: none;
            border-color: #667eea;
        }
        .btn {
            width: 100%;
            padding: 12px;
            border: none;
            border-radius: 8px;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            margin-top: 10px;
        }
        .btn-primary {
            background-color: #667eea;
            color: white;
        }
        .btn-primary:hover { background-color: #5568d3; }
        .btn-danger {
            background-color: #d9534f;
            color: white;
        }
        .btn-danger:hover { background-color: #c9302c; }
        .info-box {
            background-color: #f8f9fa;
            padding: 12px;
            border-radius: 8px;
            margin-top: 10px;
            font-size: 13px;
            color: #6c757d;
        }
        .warning-box {
            background-color: #fff3cd;
            border: 1px solid #ffc107;
            padding: 12px;
            border-radius: 8px;
            margin-top: 10px;
            font-size: 13px;
            color: #856404;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>Settings</h1>
            <p>Configure device settings</p>
        </div>
        <div class="nav">
            <a href="/">← Back to Control Panel</a>
        </div>
        <div class="content">
            <div class="settings-section">
                <h2>Device Name</h2>
                <p>Set a custom name for this device. This name is displayed in the control panel.</p>

                <div class="form-group">
                    <label for="deviceName">Device Name</label>
                    <input type="text" id="deviceName" placeholder="Enter device name" maxlength="31">
                </div>

                <button class="btn btn-primary" onclick="updateDeviceName()">Update Name</button>

                <div class="info-box">
                    <strong>Current:</strong> <span id="currentDeviceName">Loading...</span>
                </div>
            </div>

            <div class="settings-section">
                <h2>Hardware Configuration</h2>
                <p>Configure LED strip hardware settings. The device will restart after saving.</p>

                <div class="form-group">
                    <label for="numPixels">Number of LEDs</label>
                    <input type="number" id="numPixels" placeholder="Enter number of LEDs" min="1" max="1000">
                </div>

                <div class="form-group">
                    <label for="ledPin">LED Strip Pin (GPIO)</label>
                    <input type="number" id="ledPin" placeholder="Enter GPIO pin number" min="0" max="48">
                    <small style="display:block; margin-top:4px; color:#666;">Default: 39 (PIN_NEOPIXEL for onboard LED), or 35 (MOSI for external strips)</small>
                </div>

                <button class="btn btn-primary" onclick="updateHardwareSettings()">Update Hardware Settings</button>

                <div class="info-box">
                    <strong>Current:</strong> <span id="currentNumPixels">Loading...</span> LEDs on pin <span id="currentLedPin">Loading...</span><br>
                    <strong>Note:</strong> The device will restart after updating these settings.
                </div>
            </div>

            <div class="settings-section">
                <h2>Firmware Updates (OTA)</h2>
                <p>Check for and install firmware updates from GitHub releases.</p>

                <div class="info-box" id="otaCurrentVersion">
                    <strong>Current Version:</strong> <span id="currentVersion">Loading...</span><br>
                    <strong>Partition:</strong> <span id="currentPartition">Loading...</span><br>
                    <strong>Build Date:</strong> <span id="buildDate">Loading...</span>
                </div>

                <button class="btn btn-primary" onclick="checkForUpdates()" id="checkUpdateBtn">Check for Updates</button>

                <div id="updateAvailable" style="display: none; margin-top: 15px;">
                    <div class="info-box" style="background-color: #d4edda; border: 1px solid #c3e6cb; color: #155724;">
                        <strong>Update Available!</strong><br>
                        <strong>Version:</strong> <span id="latestVersion"></span><br>
                        <strong>Size:</strong> <span id="updateSize"></span><br>
                        <strong>Release Notes:</strong><br>
                        <div id="releaseNotes" style="margin-top: 5px; white-space: pre-wrap; font-size: 12px;"></div>
                    </div>
                    <button class="btn btn-primary" onclick="performUpdate()" id="installUpdateBtn">Install Update</button>
                </div>

                <div id="updateProgress" style="display: none; margin-top: 15px;">
                    <div class="info-box">
                        <strong>Installing update...</strong><br>
                        <div style="margin-top: 10px;">Please wait. The device will restart automatically after the update completes.</div>
                    </div>
                </div>

                <div class="warning-box">
                    <strong>⚠️ Note:</strong> The device will restart after installing an update. Make sure you have a stable WiFi connection before updating.
                </div>
            </div>

            <div class="settings-section">
                <h2>WiFi Configuration</h2>
                <p>Update the WiFi network credentials. The device will restart after saving.</p>

                <div class="form-group">
                    <label for="wifiSSID">WiFi Network (SSID)</label>
                    <input type="text" id="wifiSSID" placeholder="Enter WiFi network name">
                </div>

                <div class="form-group">
                    <label for="wifiPassword">WiFi Password</label>
                    <input type="password" id="wifiPassword" placeholder="Enter WiFi password">
                </div>

                <button class="btn btn-primary" onclick="updateWiFi()">Update WiFi</button>

                <div class="info-box">
                    <strong>Note:</strong> The device will restart after updating WiFi credentials. You will need to reconnect to the new network to access the device.
                </div>
            </div>

            <div class="settings-section">
                <h2>Factory Reset</h2>
                <p>Erase all settings and return the device to factory defaults.</p>

                <button class="btn btn-danger" onclick="factoryReset()">Factory Reset</button>

                <div class="warning-box">
                    <strong>⚠️ Warning:</strong> This will permanently erase all settings including:
                    <ul style="margin-top: 8px; margin-left: 20px;">
                        <li>WiFi configuration</li>
                        <li>Show settings and parameters</li>
                        <li>Strip layout configuration</li>
                        <li>All other preferences</li>
                    </ul>
                    The device will restart in AP mode and require reconfiguration.
                </div>
            </div>
        </div>
    </div>

    <script>
        // Load current device name
        async function loadDeviceName() {
            try {
                const response = await fetch('/api/status');
                const data = await response.json();
                document.getElementById('currentDeviceName').textContent = data.device_name || data.device_id;
            } catch (error) {
                console.error('Failed to load device name:', error);
                document.getElementById('currentDeviceName').textContent = 'Error loading';
            }
        }

        // Load current hardware settings from /api/about endpoint
        async function loadHardwareSettings() {
            try {
                const response = await fetch('/api/about');
                const data = await response.json();
                const numPixels = data.num_pixels || 300;
                const ledPin = data.led_pin || 35;
                document.getElementById('currentNumPixels').textContent = numPixels;
                document.getElementById('currentLedPin').textContent = ledPin;
            } catch (error) {
                console.error('Failed to load hardware settings:', error);
                document.getElementById('currentNumPixels').textContent = 'Error';
                document.getElementById('currentLedPin').textContent = 'Error';
            }
        }

        // Update device name
        async function updateDeviceName() {
            const name = document.getElementById('deviceName').value;

            if (!name || name.trim() === '') {
                alert('Please enter a device name');
                return;
            }

            try {
                const response = await fetch('/api/settings/device-name', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ name: name.trim() })
                });

                if (response.ok) {
                    alert('Device name updated successfully!');
                    document.getElementById('deviceName').value = '';
                    loadDeviceName(); // Reload current name
                } else {
                    const data = await response.json();
                    alert('Failed to update device name: ' + (data.error || 'Unknown error'));
                }
            } catch (error) {
                console.error('Failed to update device name:', error);
                alert('Error updating device name. Please try again.');
            }
        }

        // Update hardware settings (num_pixels and/or led_pin)
        async function updateHardwareSettings() {
            const numPixels = parseInt(document.getElementById('numPixels').value);
            const ledPin = parseInt(document.getElementById('ledPin').value);

            const updates = {};
            let changes = [];

            // Validate and add num_pixels if provided
            if (!isNaN(numPixels)) {
                if (numPixels < 1 || numPixels > 1000) {
                    alert('Please enter a valid number of LEDs (1-1000)');
                    return;
                }
                updates.num_pixels = numPixels;
                changes.push(`${numPixels} LEDs`);
            }

            // Validate and add led_pin if provided
            if (!isNaN(ledPin)) {
                if (ledPin < 0 || ledPin > 48) {
                    alert('Please enter a valid GPIO pin (0-48)');
                    return;
                }
                updates.led_pin = ledPin;
                changes.push(`pin ${ledPin}`);
            }

            if (changes.length === 0) {
                alert('Please enter at least one value to update');
                return;
            }

            if (!confirm(`Update hardware settings to ${changes.join(' and ')}?\n\nDevice will restart to apply changes.`)) {
                return;
            }

            try {
                const response = await fetch('/api/settings/device', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(updates)
                });

                if (response.ok) {
                    alert('Hardware settings updated!\n\nDevice is restarting...\n\nPlease wait a moment and refresh the page.');
                    document.getElementById('numPixels').value = '';
                    document.getElementById('ledPin').value = '';
                } else {
                    const data = await response.json();
                    alert('Failed to update hardware settings: ' + (data.error || 'Unknown error'));
                }
            } catch (error) {
                console.error('Failed to update hardware settings:', error);
                alert('Error updating hardware settings. Please try again.');
            }
        }

        // Update WiFi credentials
        async function updateWiFi() {
            const ssid = document.getElementById('wifiSSID').value;
            const password = document.getElementById('wifiPassword').value;

            if (!ssid || ssid.trim() === '') {
                alert('Please enter a WiFi SSID');
                return;
            }

            if (!confirm(`Update WiFi credentials to network "${ssid}"?\n\nDevice will restart and connect to the new network.`)) {
                return;
            }

            try {
                const response = await fetch('/api/settings/wifi', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ ssid, password })
                });

                if (response.ok) {
                    alert('WiFi updated successfully!\n\nDevice is restarting...\n\nPlease reconnect to the new WiFi network and access the device at its new IP address or hostname.');
                    // Clear the form
                    document.getElementById('wifiSSID').value = '';
                    document.getElementById('wifiPassword').value = '';
                } else {
                    const data = await response.json();
                    alert('Failed to update WiFi: ' + (data.error || 'Unknown error'));
                }
            } catch (error) {
                console.error('Failed to update WiFi:', error);
                alert('Error updating WiFi credentials. Please try again.');
            }
        }

        // Factory reset
        async function factoryReset() {
            if (!confirm('⚠️ Factory Reset Warning\n\nThis will erase ALL settings including:\n• WiFi configuration\n• Show settings\n• Layout configuration\n• All preferences\n\nThe device will restart in AP mode.\n\nAre you sure you want to continue?')) {
                return;
            }

            if (!confirm('⚠️ Final Confirmation\n\nThis action CANNOT be undone!\n\nAre you absolutely sure you want to factory reset?')) {
                return;
            }

            try {
                const response = await fetch('/api/settings/factory-reset', {
                    method: 'POST'
                });

                if (response.ok) {
                    alert('Factory reset complete!\n\nDevice is restarting in AP mode.\n\nConnect to the WiFi network "ledz-XXXXXX" to reconfigure the device.');
                } else {
                    alert('Failed to factory reset. Please try again.');
                }
            } catch (error) {
                console.error('Failed to factory reset:', error);
                alert('Error performing factory reset. Please try again.');
            }
        }

        // OTA Update Functions
        let updateInfo = null;

        async function loadOTAStatus() {
            try {
                const response = await fetch('/api/ota/status');
                const data = await response.json();
                document.getElementById('currentVersion').textContent = data.firmware_version || 'Unknown';
                document.getElementById('currentPartition').textContent = data.partition || 'Unknown';
                document.getElementById('buildDate').textContent = data.build_date || 'Unknown';
            } catch (error) {
                console.error('Failed to load OTA status:', error);
                document.getElementById('currentVersion').textContent = 'Error loading';
            }
        }

        async function checkForUpdates() {
            const btn = document.getElementById('checkUpdateBtn');
            btn.disabled = true;
            btn.textContent = 'Checking...';

            try {
                const response = await fetch('/api/ota/check');
                const data = await response.json();

                if (data.update_available) {
                    updateInfo = data;
                    document.getElementById('latestVersion').textContent = data.latest_version;
                    document.getElementById('updateSize').textContent = formatBytes(data.size_bytes);
                    document.getElementById('releaseNotes').textContent = data.release_notes || 'No release notes available.';
                    document.getElementById('updateAvailable').style.display = 'block';
                    btn.textContent = 'Update Available!';
                } else {
                    alert('You are running the latest version: ' + data.current_version);
                    btn.textContent = 'Check for Updates';
                    btn.disabled = false;
                }
            } catch (error) {
                console.error('Failed to check for updates:', error);
                alert('Failed to check for updates. Please ensure you have internet connection.');
                btn.textContent = 'Check for Updates';
                btn.disabled = false;
            }
        }

        async function performUpdate() {
            if (!updateInfo) {
                alert('No update information available. Please check for updates first.');
                return;
            }

            if (!confirm('Install firmware update ' + updateInfo.latest_version + '?\n\nThe device will restart after the update completes.')) {
                return;
            }

            document.getElementById('updateAvailable').style.display = 'none';
            document.getElementById('updateProgress').style.display = 'block';
            document.getElementById('checkUpdateBtn').disabled = true;

            try {
                const response = await fetch('/api/ota/update', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        download_url: updateInfo.download_url,
                        size: updateInfo.size_bytes
                    })
                });

                if (response.ok) {
                    // Wait for device to restart
                    setTimeout(() => {
                        alert('Update started! The device will restart in a few moments.\n\nPlease wait about 30 seconds, then refresh this page.');
                    }, 2000);
                } else {
                    alert('Failed to start update. Please try again.');
                    document.getElementById('updateProgress').style.display = 'none';
                    document.getElementById('checkUpdateBtn').disabled = false;
                }
            } catch (error) {
                console.error('Failed to perform update:', error);
                alert('Error performing update. Please try again.');
                document.getElementById('updateProgress').style.display = 'none';
                document.getElementById('checkUpdateBtn').disabled = false;
            }
        }

        function formatBytes(bytes) {
            if (bytes < 1024) return bytes + ' B';
            if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
            return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
        }

        // Load device info on page load
        loadDeviceName();
        loadHardwareSettings();
        loadOTAStatus();
    </script>
</body>
</html>
        )rawliteral");
    });
#endif
}

#ifdef ARDUINO
void WebServerManager::handleWiFiConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index,
                                        size_t total) {
    // Only process the first chunk (index == 0)
    if (index == 0) {
        // Parse JSON body
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, data, len);

        if (error) {
            request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
            return;
        }

        // Extract SSID and password
        const char *ssid = doc["ssid"];
        const char *password = doc["password"];

        if (ssid == nullptr || strlen(ssid) == 0) {
            request->send(400, "application/json", "{\"success\":false,\"error\":\"SSID required\"}");
            return;
        }

        // Create WiFi config
        Config::WiFiConfig wifiConfig;
        strncpy(wifiConfig.ssid, ssid, sizeof(wifiConfig.ssid) - 1);
        wifiConfig.ssid[sizeof(wifiConfig.ssid) - 1] = '\0';

        if (password != nullptr) {
            strncpy(wifiConfig.password, password, sizeof(wifiConfig.password) - 1);
            wifiConfig.password[sizeof(wifiConfig.password) - 1] = '\0';
        } else {
            wifiConfig.password[0] = '\0';
        }

        wifiConfig.configured = true;

        // Save configuration
        config.saveWiFiConfig(wifiConfig);

        Serial.print("WiFi configured: SSID=");
        Serial.println(wifiConfig.ssid);

        // Generate mDNS hostname for response
        String deviceId = DeviceId::getDeviceId();
        String hostname = "ledz-" + deviceId;
        hostname.toLowerCase();

        // Send success response with hostname
        StaticJsonDocument<128> responseDoc;
        responseDoc["success"] = true;
        responseDoc["hostname"] = hostname + ".local";

        String response;
        serializeJson(responseDoc, response);
        request->send(200, "application/json", response);

        // Note: The Network task will detect config.isConfigured() and restart the device
    }
}
#endif

void WebServerManager::begin() {
#ifdef ARDUINO
    Serial.println("Starting webserver...");

    // Add access logging middleware for all requests
    // server.addMiddleware(&logging);

    // Setup routes (implemented by subclass)
    setupRoutes();

    // Start server
    server.begin();

    Serial.println("Webserver started on port 80");
#endif
}

void WebServerManager::end() {
#ifdef ARDUINO
    server.end();
    Serial.println("Webserver stopped");
#endif
}

// ConfigWebServerManager implementation
ConfigWebServerManager::ConfigWebServerManager(Config::ConfigManager &config, Network &network,
                                               ShowController &showController)
    : WebServerManager(config, network, showController) {
}

void ConfigWebServerManager::setupRoutes() {
#ifdef ARDUINO
    // Setup config routes only (WiFi setup, OTA)
    setupConfigRoutes();

    // Captive portal: redirect all unknown requests to root
    // This makes the captive portal work on phones/tablets
    server.onNotFound([](AsyncWebServerRequest *request) {
        // Redirect to the root page for captive portal detection
        request->redirect("/");
    });
#endif
}

// OperationalWebServerManager implementation
OperationalWebServerManager::OperationalWebServerManager(Config::ConfigManager &config, Network &network,
                                                         ShowController &showController)
    : WebServerManager(config, network, showController) {
}

void OperationalWebServerManager::setupRoutes() {
#ifdef ARDUINO
    // Setup API routes (LED control, status, etc.)
    setupAPIRoutes();

    // Add 404 handler
    server.onNotFound([](AsyncWebServerRequest *request) {
        Serial.printf("[HTTP] 404 Not Found: %s\n", request->url().c_str());
        request->send(404, "text/plain", "Not found");
    });
#endif
}
