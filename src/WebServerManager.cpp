#include "WebServerManager.h"

#include <cstdio>

#include "Config.h"
#include "Log.h"
#include "Network.h"
#include "ShowController.h"
#include "ShowFactory.h"
#include "DeviceId.h"
#include "OTAUpdater.h"
#include "OTAConfig.h"
#include "TimerScheduler.h"
#include "TouchController.h"
#include "support/LocalTime.h"
#include "support/WiFiCredentials.h"

#ifdef ARDUINO
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <esp_ota_ops.h>
// Include compressed web content
#include "generated/config_gz.h"
#include "generated/control_gz.h"
#include "generated/about_gz.h"
#include "generated/settings_gz.h"
#include "generated/timers_gz.h"
#include "generated/common_gz.h"
#include "generated/favicon_gz.h"

// Content type constants
static const char* CONTENT_TYPE_HTML = "text/html";
static const char* CONTENT_TYPE_CSS = "text/css";
static const char* CONTENT_TYPE_SVG = "image/svg+xml";
static const char* CONTENT_TYPE_JSON = "application/json";

// JSON Key constants
static const char* JSON_KEY_SUCCESS = "success";
static const char* JSON_KEY_ERROR = "error";
static const char* JSON_KEY_VALUE = "value";
static const char* JSON_KEY_NAME = "name";
static const char* JSON_KEY_INDEX = "index";
static const char* JSON_KEY_SHOW_NAME = "show_name";
static const char* JSON_KEY_PARAMS = "params";
static const char* JSON_KEY_CURRENT_SHOW = "current_show";
static const char* JSON_KEY_SHOW_PARAMS = "show_params";

// Common JSON Responses
static const char* JSON_RESPONSE_SUCCESS = "{\"success\":true}";
static const char* JSON_RESPONSE_ERROR_QUEUE_FULL = "{\"success\":false,\"error\":\"Queue full\"}";

static const char* TAG = "http";

// API Paths
static const char* API_PATH_WIFI = "/api/wifi";
static const char* API_PATH_STATUS = "/api/status";
static const char* API_PATH_SHOWS = "/api/shows";
static const char* API_PATH_SHOW = "/api/show";
static const char* API_PATH_BRIGHTNESS = "/api/brightness";
static const char* API_PATH_LAYOUT = "/api/layout";
static const char* API_PATH_PRESETS = "/api/presets";
static const char* API_PATH_PRESETS_LOAD = "/api/presets/load";
static const char* API_PATH_TIMERS = "/api/timers";
static const char* API_PATH_RESTART = "/api/restart";
static const char* API_PATH_RESET = "/api/reset";
static const char* API_PATH_OTA_CHECK = "/api/ota/check";
static const char* API_PATH_OTA_UPDATE = "/api/ota/update";
#endif

// Helper functions to send gzipped responses
#ifdef ARDUINO
static void sendGzippedResponse(AsyncWebServerRequest *request, const char *contentType, const uint8_t *data, size_t len) {
    AsyncWebServerResponse *response = request->beginResponse(200, contentType, data, len);
    response->addHeader("Content-Encoding", "gzip");
    response->addHeader("Cache-Control", "max-age=86400");
    request->send(response);
}
#endif

// Web source files are in data/ directory
// Run: python3 scripts/compress_web.py to regenerate compressed headers

void AccessLogger::run(AsyncWebServerRequest *request, ArMiddlewareNext next) {
    // The first HTTP handler invocation flips the active webserver's
    // hasServedAnyRequestFlag so the Network task can satisfy
    // OTA_AUTO_CONFIRM_REQUIRE_REQUEST. Doing it in middleware means we
    // don't have to wrap every handler.
    if (WebServerManager::activeInstance) {
        WebServerManager::activeInstance->markServedRequest();
    }

    char logBuf[128];
    // Named locals, not const char*: remoteIP().toString() and url() both
    // return String temporaries that would be destroyed at the end of the
    // declaration, leaving c_str() dangling before snprintf reads it.
    const String ip = request->client()->remoteIP().toString();
    const String url = request->url();
    const char *method = request->methodToString();

    uint32_t elapsed = millis();
    next();
    elapsed = millis() - elapsed;

    AsyncWebServerResponse *response = request->getResponse();
    if (response) {
        snprintf(logBuf, sizeof(logBuf), "%s %s %s (%u ms) %u",
                 ip.c_str(), url.c_str(), method, elapsed, response->code());
    } else {
        snprintf(logBuf, sizeof(logBuf), "%s %s %s (%u ms) (no response)",
                 ip.c_str(), url.c_str(), method, elapsed);
    }
    ESP_LOGD(TAG, "%s", logBuf);
}

void WebServerManager::setupCommonRoutes() {
#ifdef ARDUINO
    // Serve common CSS (gzip compressed)
    server.on("/common.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        sendGzippedResponse(request, CONTENT_TYPE_CSS, COMMON_GZ, COMMON_GZ_LEN);
    });

    // Serve favicon (gzip compressed)
    server.on("/favicon.svg", HTTP_GET, [](AsyncWebServerRequest *request) {
        sendGzippedResponse(request, CONTENT_TYPE_SVG, FAVICON_GZ, FAVICON_GZ_LEN);
    });
#endif
}

void WebServerManager::setupConfigRoutes() {
#ifdef ARDUINO
    ESP_LOGI(TAG, "Setting up config routes...");

    setupCommonRoutes();

    // Serve WiFi config page (gzip compressed)
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        sendGzippedResponse(request, CONTENT_TYPE_HTML, CONFIG_GZ, CONFIG_GZ_LEN);
    });

    // Handle WiFi configuration POST
    {
        auto *handler = new AsyncCallbackJsonWebHandler(
            AsyncURIMatcher::exact(API_PATH_WIFI),
            [this](AsyncWebServerRequest *request, JsonVariant &doc) {
                this->handleWiFiConfig(request, doc);
            });
        handler->setMethod(HTTP_POST);
        server.addHandler(handler);
    }
#endif
}

void WebServerManager::setupAPIRoutes() {
#ifdef ARDUINO
    ESP_LOGI(TAG, "Setting up API routes...");

    setupCommonRoutes();

    // Serve main control page (gzip compressed)
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        sendGzippedResponse(request, CONTENT_TYPE_HTML, CONTROL_GZ, CONTROL_GZ_LEN);
    });

    // GET /api/status - Get device status
    server.on(API_PATH_STATUS, HTTP_GET, [this](AsyncWebServerRequest *request) {
        JsonDocument doc;

        // Device info
        Config::DeviceConfig deviceConfig = config.loadDeviceConfig();
        doc["device_id"] = deviceConfig.device_id;
        doc["device_name"] = deviceConfig.device_name;
        doc["num_pixels"] = deviceConfig.num_pixels;
        doc["led_pin"] = deviceConfig.led_pin;
        doc["brightness"] = showController.getBrightness();
        doc["cycle_time"] = deviceConfig.cycle_time;
        doc["gamma_mode"] = deviceConfig.gamma_mode;
        doc["firmware_version"] = FIRMWARE_VERSION;

        // OTA partition info
        const esp_partition_t *running_partition = esp_ota_get_running_partition();
        if (running_partition != nullptr) {
            doc["ota_partition"] = running_partition->label;
        }

        // Show info
        doc[JSON_KEY_CURRENT_SHOW] = showController.getCurrentShowName();

        // Current show configuration
        Config::ShowConfig showConfig = config.loadShowConfig();
        if (strlen(showConfig.params_json) > 0) {
            // Parse the params_json and include it
            JsonDocument paramsDoc;
            if (DeserializationError error = deserializeJson(paramsDoc, showConfig.params_json); !error) {
                doc[JSON_KEY_SHOW_PARAMS] = paramsDoc.as<JsonObject>();
            }
        }

        // Network info
        doc["wifi_connected"] = WiFiClass::status() == WL_CONNECTED;
        if (WiFiClass::status() == WL_CONNECTED) {
            doc["ip_address"] = WiFi.localIP().toString();
            doc["wifi_ssid"] = WiFi.SSID();
        }
        // Configured SSID, independent of connection state: the settings page
        // prefills from this, and needs it in AP mode too.
        Config::WiFiConfig wifiConfig = config.loadWiFiConfig();
        doc["wifi_configured_ssid"] = wifiConfig.ssid;

        String response;
        serializeJson(doc, response);
        request->send(200, CONTENT_TYPE_JSON, response);
    });

    // GET /api/shows - List available shows
    server.on(API_PATH_SHOWS, HTTP_GET, [this](AsyncWebServerRequest *request) {
        JsonDocument doc;
        JsonArray shows = doc["shows"].to<JsonArray>();

        const std::vector<ShowFactory::ShowInfo> &showList = showController.listShows();
        for (const auto &showInfo: showList) {
            JsonObject show = shows.add<JsonObject>();
            show[JSON_KEY_NAME] = showInfo.name;
            show["description"] = showInfo.description;
        }

        String response;
        serializeJson(doc, response);
        request->send(200, CONTENT_TYPE_JSON, response);
    });

    // POST /api/show - Change current show
    {
        auto *handler = new AsyncCallbackJsonWebHandler(
            AsyncURIMatcher::exact(API_PATH_SHOW),
            [this](AsyncWebServerRequest *request, JsonVariant &doc) {
                const char *showName = doc[JSON_KEY_NAME];
                if (showName == nullptr) {
                    request->send(400, CONTENT_TYPE_JSON,
                                  R"({"success":false,"error":"Show name required"})");
                    return;
                }

                // Get parameters if provided
                String paramsJson;
                if (!doc[JSON_KEY_PARAMS].isNull()) {
                    JsonObject params = doc[JSON_KEY_PARAMS];
                    serializeJson(params, paramsJson);
                } else {
                    paramsJson = "{}";
                }

                if (showController.queueShowChange(showName, paramsJson.c_str())) {
                    request->send(200, CONTENT_TYPE_JSON, JSON_RESPONSE_SUCCESS);
                } else {
                    request->send(503, CONTENT_TYPE_JSON, JSON_RESPONSE_ERROR_QUEUE_FULL);
                }
            });
        handler->setMethod(HTTP_POST);
        server.addHandler(handler);
    }

    // POST /api/brightness - Change brightness
    {
        auto *handler = new AsyncCallbackJsonWebHandler(
            AsyncURIMatcher::exact(API_PATH_BRIGHTNESS),
            [this](AsyncWebServerRequest *request, JsonVariant &doc) {
                if (doc["value"].isNull()) {
                    request->send(400, CONTENT_TYPE_JSON,
                                  R"({"success":false,"error":"Brightness value required"})");
                    return;
                }

                uint8_t brightness = doc[JSON_KEY_VALUE];
                if (showController.queueBrightnessChange(brightness)) {
                    request->send(200, CONTENT_TYPE_JSON, JSON_RESPONSE_SUCCESS);
                } else {
                    request->send(503, CONTENT_TYPE_JSON, JSON_RESPONSE_ERROR_QUEUE_FULL);
                }
            });
        handler->setMethod(HTTP_POST);
        server.addHandler(handler);
    }

    // POST /api/layout - Change strip layout configuration
    {
        auto *handler = new AsyncCallbackJsonWebHandler(
            AsyncURIMatcher::exact(API_PATH_LAYOUT),
            [this](AsyncWebServerRequest *request, JsonVariant &doc) {
                Config::LayoutConfig layoutConfig = config.loadLayoutConfig();

                // Update fields if provided
                if (!doc["reverse"].isNull()) {
                    layoutConfig.reverse = doc["reverse"];
                }
                if (!doc["mirror"].isNull()) {
                    layoutConfig.mirror = doc["mirror"];
                }
                if (!doc["dead_leds"].isNull()) {
                    layoutConfig.dead_leds = doc["dead_leds"];
                }

                // Queue the layout change for thread-safe execution
                if (showController.queueLayoutChange(layoutConfig.reverse, layoutConfig.mirror,
                                                     layoutConfig.dead_leds)) {
                    request->send(200, CONTENT_TYPE_JSON, JSON_RESPONSE_SUCCESS);
                } else {
                    request->send(503, CONTENT_TYPE_JSON, JSON_RESPONSE_ERROR_QUEUE_FULL);
                }
            });
        handler->setMethod(HTTP_POST);
        server.addHandler(handler);
    }

    // GET /api/layout - Get current layout configuration
    server.on(API_PATH_LAYOUT, HTTP_GET, [this](AsyncWebServerRequest *request) {
        Config::LayoutConfig layoutConfig = config.loadLayoutConfig();

        JsonDocument doc;
        doc["reverse"] = layoutConfig.reverse;
        doc["mirror"] = layoutConfig.mirror;
        doc["dead_leds"] = layoutConfig.dead_leds;

        String response;
        serializeJson(doc, response);
        request->send(200, CONTENT_TYPE_JSON, response);
    });

    // GET /api/presets - List all presets
    server.on(API_PATH_PRESETS, HTTP_GET, [this](AsyncWebServerRequest *request) {
        Config::PresetsConfig presetsConfig = config.loadPresetsConfig();

        JsonDocument doc;
        JsonArray presets = doc["presets"].to<JsonArray>();

        for (uint8_t i = 0; i < Config::PresetsConfig::MAX_PRESETS; i++) {
            if (presetsConfig.presets[i].valid) {
                JsonObject preset = presets.add<JsonObject>();
                preset[JSON_KEY_INDEX] = i;
                preset[JSON_KEY_NAME] = presetsConfig.presets[i].name;
                preset[JSON_KEY_SHOW_NAME] = presetsConfig.presets[i].show_name;
                preset["layout_reverse"] = presetsConfig.presets[i].layout_reverse;
                preset["layout_mirror"] = presetsConfig.presets[i].layout_mirror;
                preset["layout_dead_leds"] = presetsConfig.presets[i].layout_dead_leds;

                // Parse and include params_json
                JsonDocument paramsDoc;
                if (!deserializeJson(paramsDoc, presetsConfig.presets[i].params_json)) {
                    preset[JSON_KEY_PARAMS] = paramsDoc.as<JsonObject>();
                }
            }
        }

        String response;
        serializeJson(doc, response);
        request->send(200, CONTENT_TYPE_JSON, response);
    });

    // POST /api/presets/load - Load a preset by index or name
    // NOTE: Must be registered BEFORE /api/presets POST to avoid route conflict
    {
        auto *handler = new AsyncCallbackJsonWebHandler(
            AsyncURIMatcher::exact(API_PATH_PRESETS_LOAD),
            [this](AsyncWebServerRequest *request, JsonVariant &doc) {
                int presetIndex = -1;

                // Find preset by index or name
                if (!doc[JSON_KEY_INDEX].isNull()) {
                    presetIndex = doc[JSON_KEY_INDEX];
                    if (presetIndex < 0 || presetIndex >= Config::PresetsConfig::MAX_PRESETS) {
                        request->send(400, CONTENT_TYPE_JSON,
                                      R"({"success":false,"error":"Invalid preset index"})");
                        return;
                    }
                } else if (!doc[JSON_KEY_NAME].isNull()) {
                    const char *presetName = doc[JSON_KEY_NAME];
                    presetIndex = config.findPresetByName(presetName);
                    if (presetIndex < 0) {
                        request->send(404, CONTENT_TYPE_JSON,
                                      R"({"success":false,"error":"Preset not found"})");
                        return;
                    }
                } else {
                    request->send(400, CONTENT_TYPE_JSON,
                                  R"({"success":false,"error":"Index or name required"})");
                    return;
                }

                // Load the preset
                Config::PresetsConfig presetsConfig = config.loadPresetsConfig();
                const Config::Preset &preset = presetsConfig.presets[presetIndex];

                if (!preset.valid) {
                    request->send(404, CONTENT_TYPE_JSON,
                                  R"({"success":false,"error":"Preset slot is empty"})");
                    return;
                }

                // Queue preset load through ShowController for thread safety
                if (showController.queuePresetLoad(preset)) {
                    JsonDocument responseDoc;
                    responseDoc["success"] = true;
                    responseDoc["name"] = preset.name;
                    responseDoc["show_name"] = preset.show_name;

                    String response;
                    serializeJson(responseDoc, response);
                    request->send(200, CONTENT_TYPE_JSON, response);
                } else {
                    request->send(503, CONTENT_TYPE_JSON,
                                  R"({"success":false,"error":"Queue full"})");
                }
            });
        handler->setMethod(HTTP_POST);
        server.addHandler(handler);
    }

    // POST /api/presets - Save current state as preset
    {
        auto *handler = new AsyncCallbackJsonWebHandler(
            AsyncURIMatcher::exact(API_PATH_PRESETS),
            [this](AsyncWebServerRequest *request, JsonVariant &doc) {
                const char *presetName = doc[JSON_KEY_NAME];
                if (presetName == nullptr || strlen(presetName) == 0) {
                    request->send(400, CONTENT_TYPE_JSON,
                                  R"({"success":false,"error":"Preset name required"})");
                    return;
                }

                // Check if preset with this name already exists
                int existingIndex = config.findPresetByName(presetName);
                int slotIndex;

                if (existingIndex >= 0) {
                    // Overwrite existing preset
                    slotIndex = existingIndex;
                } else {
                    // Find next available slot
                    slotIndex = config.getNextPresetSlot();
                    if (slotIndex < 0) {
                        request->send(400, CONTENT_TYPE_JSON,
                                      R"({"success":false,"error":"All preset slots are full"})");
                        return;
                    }
                }

                // Load current state to save
                Config::ShowConfig showConfig = config.loadShowConfig();
                Config::LayoutConfig layoutConfig = config.loadLayoutConfig();

                // Create preset from current state
                Config::Preset preset;
                preset.valid = true;

                strncpy(preset.name, presetName, sizeof(preset.name) - 1);
                preset.name[sizeof(preset.name) - 1] = '\0';

                strncpy(preset.show_name, showConfig.current_show, sizeof(preset.show_name) - 1);
                preset.show_name[sizeof(preset.show_name) - 1] = '\0';

                strncpy(preset.params_json, showConfig.params_json, sizeof(preset.params_json) - 1);
                preset.params_json[sizeof(preset.params_json) - 1] = '\0';

                preset.layout_reverse = layoutConfig.reverse;
                preset.layout_mirror = layoutConfig.mirror;
                preset.layout_dead_leds = layoutConfig.dead_leds;

                if (config.savePreset(slotIndex, preset)) {
                    JsonDocument responseDoc;
                    responseDoc["success"] = true;
                    responseDoc["index"] = slotIndex;
                    responseDoc["name"] = preset.name;

                    String response;
                    serializeJson(responseDoc, response);
                    request->send(200, CONTENT_TYPE_JSON, response);
                } else {
                    request->send(500, CONTENT_TYPE_JSON,
                                  R"({"success":false,"error":"Failed to save preset"})");
                }
            });
        handler->setMethod(HTTP_POST);
        server.addHandler(handler);
    }

    // DELETE /api/presets - Delete a preset by index or name
    {
        auto *handler = new AsyncCallbackJsonWebHandler(
            AsyncURIMatcher::exact(API_PATH_PRESETS),
            [this](AsyncWebServerRequest *request, JsonVariant &doc) {
                int presetIndex = -1;

                // Find preset by index or name
                if (!doc[JSON_KEY_INDEX].isNull()) {
                    presetIndex = doc[JSON_KEY_INDEX];
                    if (presetIndex < 0 || presetIndex >= Config::PresetsConfig::MAX_PRESETS) {
                        request->send(400, CONTENT_TYPE_JSON,
                                      R"({"success":false,"error":"Invalid preset index"})");
                        return;
                    }
                } else if (!doc[JSON_KEY_NAME].isNull()) {
                    const char *presetName = doc[JSON_KEY_NAME];
                    presetIndex = config.findPresetByName(presetName);
                    if (presetIndex < 0) {
                        request->send(404, CONTENT_TYPE_JSON,
                                      R"({"success":false,"error":"Preset not found"})");
                        return;
                    }
                } else {
                    request->send(400, CONTENT_TYPE_JSON,
                                  R"({"success":false,"error":"Index or name required"})");
                    return;
                }

                // Delete the preset
                if (config.deletePreset(presetIndex)) {
                    request->send(200, CONTENT_TYPE_JSON, JSON_RESPONSE_SUCCESS);
                } else {
                    request->send(500, CONTENT_TYPE_JSON,
                                  R"({"success":false,"error":"Failed to delete preset"})");
                }
            });
        handler->setMethod(HTTP_DELETE);
        server.addHandler(handler);
    }

    // POST /api/restart - Restart the device
    server.on(API_PATH_RESTART, HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, CONTENT_TYPE_JSON, R"({"success":true,"message":"Restarting..."})");
        delay(500); // Give time for response to send
        ESP.restart();
    });

    // POST /api/settings/wifi - Update WiFi credentials
    {
        auto *handler = new AsyncCallbackJsonWebHandler(
            AsyncURIMatcher::exact("/api/settings/wifi"),
            [this](AsyncWebServerRequest *request, JsonVariant &doc) {
                const char *ssid = doc["ssid"];

                if (ssid == nullptr || strlen(ssid) == 0) {
                    request->send(400, CONTENT_TYPE_JSON, R"({"success":false,"error":"SSID required"})");
                    return;
                }

                // An absent "password" key preserves the stored password; a present
                // one (including "") overwrites it. The settings page prefills the
                // SSID but can never prefill the password, so a blank field must not
                // wipe working credentials. Rule lives in support/WiFiCredentials so
                // it can be unit-tested natively - keep it there.
                Support::WiFiCredentialUpdate update;
                update.ssid = ssid;
                update.password = !doc["password"].isNull()
                                      ? static_cast<const char *>(doc["password"])
                                      : nullptr;

                Config::WiFiConfig wifiConfig =
                    Support::mergeWiFiCredentials(config.loadWiFiConfig(), update);
                config.saveWiFiConfig(wifiConfig);

                ESP_LOGI(TAG, "WiFi credentials updated: SSID=%s", wifiConfig.ssid);

                // Send success response and restart
                request->send(200, CONTENT_TYPE_JSON,
                              R"({"success":true,"message":"WiFi updated, restarting..."})");
                delay(1000); // Give time for response to send
                ESP.restart();
            });
        handler->setMethod(HTTP_POST);
        server.addHandler(handler);
    }

    // POST /api/settings/device-name - Update device name
    {
        auto *handler = new AsyncCallbackJsonWebHandler(
            AsyncURIMatcher::exact("/api/settings/device-name"),
            [this](AsyncWebServerRequest *request, JsonVariant &doc) {
                const char *name = doc[JSON_KEY_NAME];

                if (name == nullptr || strlen(name) == 0) {
                    request->send(400, CONTENT_TYPE_JSON,
                                  R"({"success":false,"error":"Device name required"})");
                    return;
                }

                // Load current config, update name, and save
                Config::DeviceConfig deviceConfig = config.loadDeviceConfig();
                strncpy(deviceConfig.device_name, name, sizeof(deviceConfig.device_name) - 1);
                deviceConfig.device_name[sizeof(deviceConfig.device_name) - 1] = '\0';
                config.saveDeviceConfig(deviceConfig);

                ESP_LOGI(TAG, "Device name updated: %s", deviceConfig.device_name);

                // Send success response (no restart needed)
                request->send(200, CONTENT_TYPE_JSON, "{\"success\":true}");
            });
        handler->setMethod(HTTP_POST);
        server.addHandler(handler);
    }

    // POST /api/settings/device - Update device hardware settings (num_pixels, led_pin)
    {
        auto *handler = new AsyncCallbackJsonWebHandler(
            AsyncURIMatcher::exact("/api/settings/device"),
            [this](AsyncWebServerRequest *request, JsonVariant &doc) {
                // Load current config
                Config::DeviceConfig deviceConfig = config.loadDeviceConfig();
                bool changed = false;

                // Update num_pixels if provided
                if (!doc["num_pixels"].isNull()) {
                    uint16_t num_pixels = doc["num_pixels"];

                    if (num_pixels < 1 || num_pixels > 1000) {
                        request->send(400, CONTENT_TYPE_JSON,
                                      R"({"success":false,"error":"Number of pixels must be between 1 and 1000"})");
                        return;
                    }

                    deviceConfig.num_pixels = num_pixels;
                    ESP_LOGI(TAG, "Number of pixels updated: %u", num_pixels);
                    changed = true;
                }

                // Update led_pin if provided
                if (!doc["led_pin"].isNull()) {
                    uint8_t led_pin = doc["led_pin"];

                    if (led_pin > 48) {
                        request->send(400, CONTENT_TYPE_JSON,
                                      R"({"success":false,"error":"LED pin must be between 0 and 48"})");
                        return;
                    }

                    deviceConfig.led_pin = led_pin;
                    ESP_LOGI(TAG, "LED pin updated: %u", led_pin);
                    changed = true;
                }

                // Update gamma_mode if provided
                if (!doc["gamma_mode"].isNull()) {
                    int gamma_mode = doc["gamma_mode"];

                    if (gamma_mode < 0 || gamma_mode > 2) {
                        request->send(400, CONTENT_TYPE_JSON,
                                      "{\"success\":false,\"error\":\"Gamma mode must be 0 (default), 1 (NeoPixel), or 2 (none)\"}");
                        return;
                    }

                    deviceConfig.gamma_mode = static_cast<Config::GammaMode>(gamma_mode);
                    ESP_LOGI(TAG, "Gamma mode updated: %d", gamma_mode);
                    changed = true;
                }

                // Update cycle_time if provided
                if (!doc["cycle_time"].isNull()) {
                    uint16_t cycle_time = doc["cycle_time"];

                    if (cycle_time < 1 || cycle_time > 1000) {
                        request->send(400, CONTENT_TYPE_JSON,
                                      R"({"success":false,"error":"Cycle time must be between 1 and 1000"})");
                        return;
                    }

                    deviceConfig.cycle_time = cycle_time;
                    ESP_LOGI(TAG, "Cycle time updated: %u ms", cycle_time);
                    changed = true;
                }

                if (!changed) {
                    request->send(400, CONTENT_TYPE_JSON,
                                  R"({"success":false,"error":"No valid parameters provided"})");
                    return;
                }

                // Save config
                config.saveDeviceConfig(deviceConfig);

                // Send success response and request deferred restart
                request->send(200, CONTENT_TYPE_JSON,
                              R"({"success":true,"message":"Device settings updated, restarting..."})");
                config.requestRestart(1000);
            });
        handler->setMethod(HTTP_POST);
        server.addHandler(handler);
    }

    // POST /api/settings/factory-reset - Factory reset device
    server.on("/api/settings/factory-reset", HTTP_POST, [this](AsyncWebServerRequest *request) {
        ESP_LOGW(TAG, "Factory reset requested");

        // Send success response first
        request->send(200, CONTENT_TYPE_JSON,
                      R"({"success":true,"message":"Factory reset complete, restarting..."})");

        // Clear all configuration
        config.reset();

        ESP_LOGW(TAG, "All settings cleared");

        // Request deferred restart
        config.requestRestart(1000);
    });

    // GET /api/about - Device information
    server.on("/api/about", HTTP_GET, [this](AsyncWebServerRequest *request) {
        JsonDocument doc;

        // Device info
        Config::DeviceConfig deviceConfig = config.loadDeviceConfig();
        doc["device_id"] = deviceConfig.device_id;
        doc["num_pixels"] = deviceConfig.num_pixels;
        doc["led_pin"] = deviceConfig.led_pin;
        doc["cycle_time"] = deviceConfig.cycle_time;

        // Show statistics
        ShowStats stats = showController.getStats();
        JsonObject statsJson = doc["stats"].to<JsonObject>();
        statsJson["avg_execution_time"] = stats.avg_execution_time;
        statsJson["avg_show_time"] = stats.avg_show_time;
        statsJson["avg_cycle_time"] = stats.avg_cycle_time;
        statsJson["last_execution_time"] = stats.last_execution_time;
        statsJson["last_show_time"] = stats.last_show_time;

        // Chip info
        doc["chip_model"] = ESP.getChipModel();
        doc["chip_revision"] = ESP.getChipRevision();
        doc["chip_cores"] = ESP.getChipCores();
        doc["cpu_freq_mhz"] = ESP.getCpuFreqMHz();
#ifdef ARDUINO
        doc["cpu_temp"] = temperatureRead();
#endif

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
        if (WiFiClass::status() == WL_CONNECTED) {
            doc["wifi_ssid"] = WiFi.SSID();
            doc["wifi_rssi"] = WiFi.RSSI();
            doc["ip_address"] = WiFi.localIP().toString();
            doc["mac_address"] = WiFi.macAddress();
            doc["wifi_tx_power"] = WiFi.getTxPower();
            doc["wifi_sleep_mode"] = WiFi.getSleep();
        } else if (WiFiClass::getMode() == WIFI_AP) {
            doc["ap_ssid"] = WiFi.softAPgetHostname();
            doc["ap_ip"] = WiFi.softAPIP().toString();
            doc["ap_clients"] = WiFi.softAPgetStationNum();
        }

        String response;
        serializeJson(doc, response);
        request->send(200, CONTENT_TYPE_JSON, response);
    });

    // GET /api/timers - List all timers with remaining time
    server.on(API_PATH_TIMERS, HTTP_GET, [this](AsyncWebServerRequest *request) {
        TimerScheduler *scheduler = network.getTimerScheduler();
        if (!scheduler) {
            request->send(503, CONTENT_TYPE_JSON, R"({"success":false,"error":"Timer scheduler not available"})");
            return;
        }

        uint32_t currentEpoch = network.getCurrentEpoch();
        const Config::TimersConfig &timersConfig = scheduler->getTimersConfig();

        JsonDocument doc;
        doc["timezone"] = timersConfig.timezone;
        doc["current_epoch"] = currentEpoch;

        // Include local time as seconds since midnight for UI convenience.
        // Without NTP there is no instant to resolve the zone at, so the
        // derived fields are reported as unknown rather than describing
        // epoch 0.
        // Outlives the document: ArduinoJson stores a const char* by
        // reference, so tz_abbrev must not point at a dead local.
        LocalTime::Info tzInfo = {};
        if (currentEpoch != 0) {
            doc["local_seconds_since_midnight"] = scheduler->getSecondsSinceMidnight(currentEpoch);

            tzInfo = LocalTime::describe(currentEpoch, timersConfig.timezone);
            doc["tz_abbrev"] = static_cast<const char *>(tzInfo.abbrev);
            doc["tz_offset_minutes"] = tzInfo.offset_minutes;
            doc["is_dst"] = tzInfo.is_dst;
        } else {
            doc["local_seconds_since_midnight"] = 0;
        }

        JsonArray timers = doc["timers"].to<JsonArray>();
        for (uint8_t i = 0; i < Config::TimersConfig::MAX_TIMERS; i++) {
            const Config::TimerEntry &timer = timersConfig.timers[i];
            JsonObject timerObj = timers.add<JsonObject>();
            timerObj["index"] = i;
            timerObj["enabled"] = timer.enabled;

            if (timer.enabled) {
                timerObj["type"] = static_cast<int>(timer.type);
                timerObj["action"] = static_cast<int>(timer.action);
                timerObj["preset_index"] = timer.preset_index;
                timerObj["target_time"] = timer.target_time;
                timerObj["duration_seconds"] = timer.duration_seconds;
                timerObj["remaining_seconds"] = scheduler->getRemainingSeconds(i, currentEpoch);

                // Add type name for UI convenience
                switch (timer.type) {
                    case Config::TimerType::COUNTDOWN:
                        timerObj["type_name"] = "countdown";
                        break;
                    case Config::TimerType::ALARM_DAILY:
                        timerObj["type_name"] = "alarm_daily";
                        break;
                }
            }
        }

        String response;
        serializeJson(doc, response);
        request->send(200, CONTENT_TYPE_JSON, response);
    });

    // POST /api/timers/countdown - Set a countdown timer
    {
        auto *handler = new AsyncCallbackJsonWebHandler(
            AsyncURIMatcher::exact("/api/timers/countdown"),
            [this](AsyncWebServerRequest *request, JsonVariant &doc) {
                TimerScheduler *scheduler = network.getTimerScheduler();
                if (!scheduler) {
                    request->send(503, CONTENT_TYPE_JSON,
                                  R"({"success":false,"error":"Timer scheduler not available"})");
                    return;
                }



                // Required: duration in seconds
                if (doc["duration"].isNull()) {
                    request->send(400, CONTENT_TYPE_JSON,
                                  R"({"success":false,"error":"Duration required"})");
                    return;
                }

                uint32_t duration = doc["duration"];
                if (duration == 0 || duration > 86400 * 7) { // Max 7 days
                    request->send(400, CONTENT_TYPE_JSON,
                                  R"({"success":false,"error":"Invalid duration"})");
                    return;
                }

                // Optional: index (defaults to first available slot)
                int timerIndex = doc[JSON_KEY_INDEX] | -1;
                if (timerIndex == -1) {
                    // Find first available slot
                    const Config::TimersConfig &timersConfig = scheduler->getTimersConfig();
                    for (uint8_t i = 0; i < Config::TimersConfig::MAX_TIMERS; i++) {
                        if (!timersConfig.timers[i].enabled) {
                            timerIndex = i;
                            break;
                        }
                    }
                    if (timerIndex == -1) {
                        request->send(400, CONTENT_TYPE_JSON,
                                      R"({"success":false,"error":"All timer slots are full"})");
                        return;
                    }
                }

                // Optional: action (defaults to TURN_OFF)
                Config::TimerAction action = Config::TimerAction::TURN_OFF;
                if (!doc["action"].isNull()) {
                    int actionInt = doc["action"];
                    if (actionInt == 0) action = Config::TimerAction::LOAD_PRESET;
                    else action = Config::TimerAction::TURN_OFF;
                }

                // Optional: preset_index (only used if action is LOAD_PRESET)
                uint8_t presetIndex = doc["preset_index"] | 0;

                uint32_t currentEpoch = network.getCurrentEpoch();
                if (scheduler->setCountdown(timerIndex, duration, action, presetIndex, currentEpoch)) {
                    JsonDocument responseDoc;
                    responseDoc["success"] = true;
                    responseDoc["index"] = timerIndex;
                    responseDoc["remaining_seconds"] = duration;

                    String response;
                    serializeJson(responseDoc, response);
                    request->send(200, CONTENT_TYPE_JSON, response);
                } else {
                    request->send(500, CONTENT_TYPE_JSON,
                                  R"({"success":false,"error":"Failed to set timer"})");
                }
            });
        handler->setMethod(HTTP_POST);
        server.addHandler(handler);
    }

    // POST /api/timers/alarm - Set a daily recurring alarm
    {
        auto *handler = new AsyncCallbackJsonWebHandler(
            AsyncURIMatcher::exact("/api/timers/alarm"),
            [this](AsyncWebServerRequest *request, JsonVariant &doc) {
                TimerScheduler *scheduler = network.getTimerScheduler();
                if (!scheduler) {
                    request->send(503, CONTENT_TYPE_JSON,
                                  R"({"success":false,"error":"Timer scheduler not available"})");
                    return;
                }



                // Required: hour and minute for the alarm time
                if (doc["hour"].isNull() || doc["minute"].isNull()) {
                    request->send(400, CONTENT_TYPE_JSON,
                                  R"({"success":false,"error":"Hour and minute required"})");
                    return;
                }

                uint8_t hour = doc["hour"];
                uint8_t minute = doc["minute"];
                if (hour > 23 || minute > 59) {
                    request->send(400, CONTENT_TYPE_JSON,
                                  R"({"success":false,"error":"Invalid time"})");
                    return;
                }

                // Optional: index (defaults to first available slot)
                int timerIndex = doc[JSON_KEY_INDEX] | -1;
                if (timerIndex == -1) {
                    const Config::TimersConfig &timersConfig = scheduler->getTimersConfig();
                    for (uint8_t i = 0; i < Config::TimersConfig::MAX_TIMERS; i++) {
                        if (!timersConfig.timers[i].enabled) {
                            timerIndex = i;
                            break;
                        }
                    }
                    if (timerIndex == -1) {
                        request->send(400, CONTENT_TYPE_JSON,
                                      R"({"success":false,"error":"All timer slots are full"})");
                        return;
                    }
                }

                // Optional: action (defaults to TURN_OFF)
                Config::TimerAction action = Config::TimerAction::TURN_OFF;
                if (!doc["action"].isNull()) {
                    int actionInt = doc["action"];
                    if (actionInt == 0) action = Config::TimerAction::LOAD_PRESET;
                    else action = Config::TimerAction::TURN_OFF;
                }

                uint8_t presetIndex = doc["preset_index"] | 0;

                uint32_t secondsSinceMidnight = hour * 3600 + minute * 60;
                bool success = scheduler->setDailyAlarm(timerIndex, secondsSinceMidnight, action, presetIndex);

                if (success) {
                    JsonDocument responseDoc;
                    responseDoc["success"] = true;
                    responseDoc["index"] = timerIndex;

                    String response;
                    serializeJson(responseDoc, response);
                    request->send(200, CONTENT_TYPE_JSON, response);
                } else {
                    request->send(500, CONTENT_TYPE_JSON,
                                  R"({"success":false,"error":"Failed to set alarm"})");
                }
            });
        handler->setMethod(HTTP_POST);
        server.addHandler(handler);
    }

    // DELETE /api/timers - Cancel a timer by index
    {
        auto *handler = new AsyncCallbackJsonWebHandler(
            AsyncURIMatcher::exact(API_PATH_TIMERS),
            [this](AsyncWebServerRequest *request, JsonVariant &doc) {
                TimerScheduler *scheduler = network.getTimerScheduler();
                if (!scheduler) {
                    request->send(503, CONTENT_TYPE_JSON,
                                  R"({"success":false,"error":"Timer scheduler not available"})");
                    return;
                }



                if (doc[JSON_KEY_INDEX].isNull()) {
                    request->send(400, CONTENT_TYPE_JSON,
                                  R"({"success":false,"error":"Timer index required"})");
                    return;
                }

                int timerIndex = doc[JSON_KEY_INDEX];
                if (timerIndex < 0 || timerIndex >= Config::TimersConfig::MAX_TIMERS) {
                    request->send(400, CONTENT_TYPE_JSON,
                                  R"({"success":false,"error":"Invalid timer index"})");
                    return;
                }

                if (scheduler->cancelTimer(timerIndex)) {
                    request->send(200, CONTENT_TYPE_JSON, JSON_RESPONSE_SUCCESS);
                } else {
                    request->send(500, CONTENT_TYPE_JSON,
                                  R"({"success":false,"error":"Failed to cancel timer"})");
                }
            });
        handler->setMethod(HTTP_DELETE);
        server.addHandler(handler);
    }

    // POST /api/timers/timezone - Set the POSIX TZ string
    {
        auto *handler = new AsyncCallbackJsonWebHandler(
            AsyncURIMatcher::exact("/api/timers/timezone"),
            [this](AsyncWebServerRequest *request, JsonVariant &doc) {
                TimerScheduler *scheduler = network.getTimerScheduler();
                if (!scheduler) {
                    request->send(503, CONTENT_TYPE_JSON,
                                  R"({"success":false,"error":"Timer scheduler not available"})");
                    return;
                }



                if (doc["tz"].isNull()) {
                    request->send(400, CONTENT_TYPE_JSON,
                                  R"({"success":false,"error":"Timezone string required"})");
                    return;
                }

                const char *tz = doc["tz"];
                if (!scheduler->setTimezone(tz)) {
                    request->send(400, CONTENT_TYPE_JSON,
                                  R"({"success":false,"error":"Invalid POSIX timezone string"})");
                    return;
                }

                request->send(200, CONTENT_TYPE_JSON, JSON_RESPONSE_SUCCESS);
            });
        handler->setMethod(HTTP_POST);
        server.addHandler(handler);
    }

    // GET /api/touch - Get touch configuration and current values
    server.on("/api/touch", HTTP_GET, [this](AsyncWebServerRequest *request) {
        TouchController *touch = network.getTouchController();
        if (!touch) {
            request->send(503, CONTENT_TYPE_JSON,
                          R"({"success":false,"error":"Touch controller not available"})");
            return;
        }

        JsonDocument doc;
        const Config::TouchConfig &touchConfig = touch->getTouchConfig();

        doc["enabled"] = touchConfig.enabled;
        doc["threshold"] = touchConfig.threshold;

        // Pin mappings
        JsonArray pins = doc["pins"].to<JsonArray>();
        for (uint8_t i = 0; i < Config::TouchConfig::MAX_TOUCH_PINS; i++) {
            JsonObject pin = pins.add<JsonObject>();
            pin["index"] = i;
            pin["gpio"] = TouchController::getGpioPin(i);
            pin["action"] = (i == 0) ? "Switch Show" : (i == 1) ? "Switch Variant" : "Switch Layout";
        }

        // Current touch values for debugging/calibration
        uint32_t touchValues[Config::TouchConfig::MAX_TOUCH_PINS];
        touch->getTouchValues(touchValues);
        JsonArray values = doc["values"].to<JsonArray>();
        for (uint8_t i = 0; i < Config::TouchConfig::MAX_TOUCH_PINS; i++) {
            values.add(touchValues[i]);
        }

        String response;
        serializeJson(doc, response);
        request->send(200, CONTENT_TYPE_JSON, response);
    });

    // POST /api/touch - Update touch configuration
    {
        auto *handler = new AsyncCallbackJsonWebHandler(
            AsyncURIMatcher::exact("/api/touch"),
            [this](AsyncWebServerRequest *request, JsonVariant &doc) {
                TouchController *touch = network.getTouchController();
                if (!touch) {
                    request->send(503, CONTENT_TYPE_JSON,
                                  R"({"success":false,"error":"Touch controller not available"})");
                    return;
                }



                Config::TouchConfig touchConfig = touch->getTouchConfig();

                // Update enabled state if provided
                if (!doc["enabled"].isNull()) {
                    touchConfig.enabled = doc["enabled"];
                }

                // Update threshold if provided
                if (!doc["threshold"].isNull()) {
                    touchConfig.threshold = doc["threshold"];
                }

                touch->setTouchConfig(touchConfig);
                request->send(200, CONTENT_TYPE_JSON, JSON_RESPONSE_SUCCESS);
            });
        handler->setMethod(HTTP_POST);
        server.addHandler(handler);
    }

    // GET /api/ota/check - kick off a background check on Core 1
    server.on(API_PATH_OTA_CHECK, HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        bool started = OTAUpdater::startBackgroundCheck(OTA_GITHUB_OWNER, OTA_GITHUB_REPO);
        if (started) {
            doc["started"] = true;
            request->send(202, CONTENT_TYPE_JSON, "{\"started\":true}");
        } else {
            doc["started"] = false;
            doc["error"] = "OTA already in progress";
            String body;
            serializeJson(doc, body);
            request->send(409, CONTENT_TYPE_JSON, body);
        }
    });

    // POST /api/ota/update - kick off a background install on Core 1
    server.on(API_PATH_OTA_UPDATE, HTTP_POST, [](AsyncWebServerRequest *request) {
        bool force = request->hasParam("force") &&
                     (request->getParam("force")->value() == String("true") ||
                      request->getParam("force")->value() == String("1"));

        bool started = OTAUpdater::startBackgroundUpdateFromLatestCheck(force);
        if (started) {
            request->send(202, CONTENT_TYPE_JSON, "{\"started\":true}");
        } else {
            String reason = "OTA already in progress or no completed check";
            CheckState cs = OTAUpdater::getCheckState();
            if (cs == CheckState::Done) {
                reason = "Latest version is not newer than running (use ?force=true to override)";
            } else if (cs == CheckState::InProgress) {
                reason = "A check is still running";
            } else if (cs == CheckState::Failed) {
                reason = "Latest check failed; retry";
            }
            JsonDocument doc;
            doc["started"] = false;
            doc["error"] = reason;
            String body;
            serializeJson(doc, body);
            request->send(409, CONTENT_TYPE_JSON, body);
        }
    });

    // GET /api/ota/status - state-machine snapshot for the UI to poll
    server.on("/api/ota/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;

        doc["firmware_version"] = FIRMWARE_VERSION;
        doc["build_date"] = FIRMWARE_BUILD_DATE;
        doc["build_time"] = FIRMWARE_BUILD_TIME;

        String partitionLabel;
        uint32_t partitionAddress;
        if (OTAUpdater::getRunningPartitionInfo(partitionLabel, partitionAddress)) {
            doc["partition"] = partitionLabel;
            doc["partition_address"] = partitionAddress;
        }
        doc["unconfirmed_update"] = OTAUpdater::hasUnconfirmedUpdate();

        uint32_t freeHeap, minFreeHeap, psramFree;
        OTAUpdater::getMemoryInfo(freeHeap, minFreeHeap, psramFree);
        doc["free_heap"] = freeHeap;
        doc["min_free_heap"] = minFreeHeap;
        doc["psram_free"] = psramFree;
        doc["ota_safe"] = OTAUpdater::hasEnoughMemory();

        // check sub-object
        {
            JsonObject chk = doc["check"].to<JsonObject>();
            CheckState cs = OTAUpdater::getCheckState();
            switch (cs) {
                case CheckState::Idle:       chk["state"] = "idle"; break;
                case CheckState::InProgress: chk["state"] = "in_progress"; break;
                case CheckState::Done:       chk["state"] = "done"; break;
                case CheckState::Failed:     chk["state"] = "failed"; break;
            }
            if (cs == CheckState::Done || cs == CheckState::InProgress) {
                FirmwareInfo info = OTAUpdater::getCheckResult();
                if (info.isValid) {
                    chk["version"] = info.version;
                    chk["name"] = info.name;
                    chk["size_bytes"] = info.size;
                    chk["download_url"] = info.downloadUrl;
                    chk["changelog"] = info.changelog;
                }
            }
        }

        // update sub-object
        {
            JsonObject upd = doc["update"].to<JsonObject>();
            Progress p = OTAUpdater::getProgress();
            switch (p.state) {
                case UpdateState::Idle:        upd["state"] = "idle"; break;
                case UpdateState::Downloading: upd["state"] = "downloading"; break;
                case UpdateState::Flashing:    upd["state"] = "flashing"; break;
                case UpdateState::Pending:     upd["state"] = "pending"; break;
                case UpdateState::Failed:      upd["state"] = "failed"; break;
            }
            upd["percent"] = p.percent;
            upd["bytes_written"] = p.bytes_written;
            upd["expected_bytes"] = p.expected_bytes;
            upd["started_at_ms"] = p.started_at_ms;
            if (!p.error_message.isEmpty()) upd["error"] = p.error_message;
        }

        String response;
        serializeJson(doc, response);
        request->send(200, CONTENT_TYPE_JSON, response);
    });

    // POST /api/ota/confirm - manual boot confirmation (escape hatch)
    server.on("/api/ota/confirm", HTTP_POST, [](AsyncWebServerRequest *request) {
        bool success = OTAUpdater::confirmBoot();
        JsonDocument doc;
        doc[JSON_KEY_SUCCESS] = success;
        doc["message"] = success ? "Boot confirmed, rollback disabled" : "Failed to confirm boot";
        String response;
        serializeJson(doc, response);
        request->send(success ? 200 : 500, CONTENT_TYPE_JSON, response);
    });

    // GET /about - About page
    server.on("/about", HTTP_GET, [](AsyncWebServerRequest *request) {
        sendGzippedResponse(request, CONTENT_TYPE_HTML, ABOUT_GZ, ABOUT_GZ_LEN);
    });

    // GET /settings - Settings page
    server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
        sendGzippedResponse(request, CONTENT_TYPE_HTML, SETTINGS_GZ, SETTINGS_GZ_LEN);
    });

    // GET /timers - Timers page
    server.on("/timers", HTTP_GET, [](AsyncWebServerRequest *request) {
        sendGzippedResponse(request, CONTENT_TYPE_HTML, TIMERS_GZ, TIMERS_GZ_LEN);
    });
#endif
}

WebServerManager *WebServerManager::activeInstance = nullptr;

WebServerManager::WebServerManager(Config::ConfigManager &config, Network &network, ShowController &show_controller)
    : config(config), network(network), showController(show_controller)
#ifdef ARDUINO
      , server(80)
#endif
{
}

#ifdef ARDUINO
void WebServerManager::handleWiFiConfig(AsyncWebServerRequest *request, JsonVariant &doc) {
    // Extract SSID and password
    const char *ssid = doc["ssid"];
    const char *password = doc["password"];

    if (ssid == nullptr || strlen(ssid) == 0) {
        request->send(400, CONTENT_TYPE_JSON, R"({"success":false,"error":"SSID required"})");
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

    ESP_LOGI(TAG, "WiFi configured: SSID=%s", wifiConfig.ssid);

    // Generate mDNS hostname for response
    String deviceId = DeviceId::getDeviceId();
    String hostname = "ledz-" + deviceId;
    hostname.toLowerCase();

    // Send success response with hostname
    JsonDocument responseDoc;
    responseDoc["success"] = true;
    responseDoc["hostname"] = hostname + ".local";

    String response;
    serializeJson(responseDoc, response);
    request->send(200, CONTENT_TYPE_JSON, response);

    // Note: The Network task will detect config.isConfigured() and restart the device
}
#endif

void WebServerManager::begin() {
#ifdef ARDUINO
    ESP_LOGI(TAG, "Starting webserver...");

    activeInstance = this;

    // Add access logging middleware for all requests. This is also the only
    // caller of markServedRequest(), so OTA_AUTO_CONFIRM_REQUIRE_REQUEST
    // cannot be satisfied while it is not installed. The chain wraps whichever
    // handler the router picked, so AsyncCallbackJsonWebHandler routes
    // registered via addHandler() are covered identically to server.on() ones.
    server.addMiddleware(&logging);

    // Setup routes (implemented by subclass)
    setupRoutes();

    // Start server
    server.begin();

    ESP_LOGI(TAG, "Webserver started on port 80");
#endif
}

void WebServerManager::end() {
#ifdef ARDUINO
    server.end();
    if (activeInstance == this) activeInstance = nullptr;
    ESP_LOGI(TAG, "Webserver stopped");
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
        ESP_LOGW(TAG, "404 Not Found: %s", request->url().c_str());
        request->send(404, "text/plain", "Not found");
    });
#endif
}
