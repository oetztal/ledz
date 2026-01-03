//
// Created by Claude Code on 03.01.26.
//

#include "WebServerManager.h"
#include "Config.h"
#include "Network.h"
#include "ShowController.h"
#include "ShowFactory.h"

#ifdef ARDUINO
#include <ArduinoJson.h>
#endif

// Simple WiFi configuration HTML page (embedded)
const char CONFIG_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>LED Controller - WiFi Setup</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 500px;
            margin: 50px auto;
            padding: 20px;
            background-color: #f0f0f0;
        }
        .container {
            background-color: white;
            padding: 30px;
            border-radius: 10px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        h1 {
            color: #333;
            text-align: center;
        }
        .form-group {
            margin-bottom: 20px;
        }
        label {
            display: block;
            margin-bottom: 5px;
            color: #555;
            font-weight: bold;
        }
        input[type="text"],
        input[type="password"] {
            width: 100%;
            padding: 10px;
            border: 1px solid #ddd;
            border-radius: 5px;
            box-sizing: border-box;
            font-size: 14px;
        }
        button {
            width: 100%;
            padding: 12px;
            background-color: #4CAF50;
            color: white;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            font-size: 16px;
            font-weight: bold;
        }
        button:hover {
            background-color: #45a049;
        }
        .status {
            margin-top: 20px;
            padding: 10px;
            border-radius: 5px;
            display: none;
        }
        .status.success {
            background-color: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
        }
        .status.error {
            background-color: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>LED Controller</h1>
        <h2 style="text-align: center; color: #666; font-size: 18px;">WiFi Configuration</h2>

        <form id="wifiForm">
            <div class="form-group">
                <label for="ssid">WiFi Network Name (SSID):</label>
                <input type="text" id="ssid" name="ssid" required placeholder="Enter WiFi SSID">
            </div>

            <div class="form-group">
                <label for="password">WiFi Password:</label>
                <input type="password" id="password" name="password" placeholder="Enter WiFi Password">
            </div>

            <button type="submit">Connect to WiFi</button>
        </form>

        <div id="status" class="status"></div>
    </div>

    <script>
        document.getElementById('wifiForm').addEventListener('submit', async (e) => {
            e.preventDefault();

            const ssid = document.getElementById('ssid').value;
            const password = document.getElementById('password').value;
            const statusDiv = document.getElementById('status');

            try {
                const response = await fetch('/api/wifi', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json',
                    },
                    body: JSON.stringify({ ssid, password })
                });

                const result = await response.json();

                if (result.success) {
                    statusDiv.className = 'status success';
                    statusDiv.textContent = 'WiFi configured successfully! Device will restart and connect to your network.';
                    statusDiv.style.display = 'block';
                } else {
                    statusDiv.className = 'status error';
                    statusDiv.textContent = 'Error: ' + (result.error || 'Unknown error');
                    statusDiv.style.display = 'block';
                }
            } catch (error) {
                statusDiv.className = 'status error';
                statusDiv.textContent = 'Error: Could not connect to device';
                statusDiv.style.display = 'block';
            }
        });
    </script>
</body>
</html>
)rawliteral";

// Main control HTML page (embedded)
const char CONTROL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>LED Controller</title>
    <style>
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
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
        .header h1 {
            font-size: 28px;
            margin-bottom: 10px;
        }
        .device-id {
            font-size: 14px;
            opacity: 0.9;
        }
        .content {
            padding: 30px;
        }
        .status-bar {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 15px;
            background-color: #f8f9fa;
            border-radius: 10px;
            margin-bottom: 30px;
        }
        .status-item {
            text-align: center;
        }
        .status-label {
            font-size: 12px;
            color: #6c757d;
            text-transform: uppercase;
            margin-bottom: 5px;
        }
        .status-value {
            font-size: 18px;
            font-weight: bold;
            color: #333;
        }
        .control-group {
            margin-bottom: 30px;
        }
        .control-label {
            display: block;
            font-size: 14px;
            font-weight: 600;
            color: #333;
            margin-bottom: 10px;
        }
        select, input[type="range"] {
            width: 100%;
            padding: 12px;
            border: 2px solid #e9ecef;
            border-radius: 8px;
            font-size: 16px;
            transition: border-color 0.3s;
        }
        select:focus {
            outline: none;
            border-color: #667eea;
        }
        input[type="range"] {
            height: 8px;
            -webkit-appearance: none;
            appearance: none;
            background: #e9ecef;
            outline: none;
            padding: 0;
        }
        input[type="range"]::-webkit-slider-thumb {
            -webkit-appearance: none;
            appearance: none;
            width: 24px;
            height: 24px;
            background: #667eea;
            cursor: pointer;
            border-radius: 50%;
        }
        input[type="range"]::-moz-range-thumb {
            width: 24px;
            height: 24px;
            background: #667eea;
            cursor: pointer;
            border-radius: 50%;
            border: none;
        }
        .brightness-value {
            text-align: right;
            font-size: 14px;
            color: #6c757d;
            margin-top: 5px;
        }
        .toggle-switch {
            position: relative;
            display: inline-block;
            width: 60px;
            height: 34px;
        }
        .toggle-switch input {
            opacity: 0;
            width: 0;
            height: 0;
        }
        .slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background-color: #ccc;
            transition: .4s;
            border-radius: 34px;
        }
        .slider:before {
            position: absolute;
            content: "";
            height: 26px;
            width: 26px;
            left: 4px;
            bottom: 4px;
            background-color: white;
            transition: .4s;
            border-radius: 50%;
        }
        input:checked + .slider {
            background-color: #667eea;
        }
        input:checked + .slider:before {
            transform: translateX(26px);
        }
        .auto-cycle-row {
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .show-description {
            font-size: 14px;
            color: #6c757d;
            margin-top: 5px;
            font-style: italic;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>LED Controller</h1>
            <div class="device-id" id="deviceId">Loading...</div>
        </div>

        <div class="content">
            <div class="status-bar">
                <div class="status-item">
                    <div class="status-label">Current Show</div>
                    <div class="status-value" id="currentShow">-</div>
                </div>
                <div class="status-item">
                    <div class="status-label">Brightness</div>
                    <div class="status-value" id="currentBrightness">-</div>
                </div>
            </div>

            <div class="control-group">
                <label class="control-label" for="showSelect">Select Show</label>
                <select id="showSelect">
                    <option value="">Loading shows...</option>
                </select>
                <div class="show-description" id="showDescription"></div>
            </div>

            <div class="control-group">
                <label class="control-label" for="brightnessSlider">Brightness</label>
                <input type="range" id="brightnessSlider" min="0" max="255" value="128">
                <div class="brightness-value">
                    <span id="brightnessValue">128</span> / 255
                </div>
            </div>

            <div class="control-group">
                <div class="auto-cycle-row">
                    <label class="control-label">Auto-Cycle Shows</label>
                    <label class="toggle-switch">
                        <input type="checkbox" id="autoCycleToggle">
                        <span class="slider"></span>
                    </label>
                </div>
            </div>
        </div>
    </div>

    <script>
        let shows = [];
        let currentStatus = {};

        // Fetch available shows
        async function loadShows() {
            try {
                const response = await fetch('/api/shows');
                const data = await response.json();
                shows = data.shows;

                const select = document.getElementById('showSelect');
                select.innerHTML = shows.map(show =>
                    `<option value="${show.name}">${show.name}</option>`
                ).join('');

                // Update description on change
                select.addEventListener('change', () => {
                    const selectedShow = shows.find(s => s.name === select.value);
                    document.getElementById('showDescription').textContent =
                        selectedShow ? selectedShow.description : '';
                });
            } catch (error) {
                console.error('Failed to load shows:', error);
            }
        }

        // Fetch device status
        async function updateStatus() {
            try {
                const response = await fetch('/api/status');
                currentStatus = await response.json();

                document.getElementById('deviceId').textContent = currentStatus.device_id;
                document.getElementById('currentShow').textContent = currentStatus.current_show;
                document.getElementById('currentBrightness').textContent = currentStatus.brightness;

                // Update UI controls without triggering change events
                const showSelect = document.getElementById('showSelect');
                if (showSelect.value !== currentStatus.current_show) {
                    showSelect.value = currentStatus.current_show;
                    const selectedShow = shows.find(s => s.name === currentStatus.current_show);
                    document.getElementById('showDescription').textContent =
                        selectedShow ? selectedShow.description : '';
                }

                const brightnessSlider = document.getElementById('brightnessSlider');
                if (Math.abs(brightnessSlider.value - currentStatus.brightness) > 5) {
                    brightnessSlider.value = currentStatus.brightness;
                    document.getElementById('brightnessValue').textContent = currentStatus.brightness;
                }

                const autoCycleToggle = document.getElementById('autoCycleToggle');
                autoCycleToggle.checked = currentStatus.auto_cycle;
            } catch (error) {
                console.error('Failed to update status:', error);
            }
        }

        // Show change handler
        document.getElementById('showSelect').addEventListener('change', async (e) => {
            const showName = e.target.value;
            try {
                await fetch('/api/show', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ name: showName })
                });
            } catch (error) {
                console.error('Failed to change show:', error);
            }
        });

        // Brightness change handler (with debouncing)
        let brightnessTimeout;
        document.getElementById('brightnessSlider').addEventListener('input', (e) => {
            const value = e.target.value;
            document.getElementById('brightnessValue').textContent = value;

            clearTimeout(brightnessTimeout);
            brightnessTimeout = setTimeout(async () => {
                try {
                    await fetch('/api/brightness', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({ value: parseInt(value) })
                    });
                } catch (error) {
                    console.error('Failed to change brightness:', error);
                }
            }, 300);
        });

        // Auto-cycle toggle handler
        document.getElementById('autoCycleToggle').addEventListener('change', async (e) => {
            try {
                await fetch('/api/auto-cycle', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ enabled: e.target.checked })
                });
            } catch (error) {
                console.error('Failed to toggle auto-cycle:', error);
            }
        });

        // Initialize
        loadShows();
        updateStatus();

        // Update status every 2 seconds
        setInterval(updateStatus, 2000);
    </script>
</body>
</html>
)rawliteral";

WebServerManager::WebServerManager(Config::ConfigManager &config, Network &network)
    : config(config), network(network), showController(nullptr), showFactory(nullptr)
#ifdef ARDUINO
    , server(80)
#endif
{
}

void WebServerManager::setShowController(ShowController *controller, ShowFactory *factory) {
    this->showController = controller;
    this->showFactory = factory;
}

void WebServerManager::setupConfigRoutes() {
#ifdef ARDUINO
    // Serve main control page or config page based on configuration status
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (config.isConfigured()) {
            request->send(200, "text/html", CONTROL_HTML);
        } else {
            request->send(200, "text/html", CONFIG_HTML);
        }
    });

    // WiFi configuration page (always accessible)
    server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", CONFIG_HTML);
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
    // GET /api/status - Get device status
    server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (showController == nullptr) {
            request->send(500, "application/json", "{\"error\":\"Controller not initialized\"}");
            return;
        }

        StaticJsonDocument<512> doc;

        // Device info
        Config::DeviceConfig deviceConfig = config.loadDeviceConfig();
        doc["device_id"] = deviceConfig.device_id;
        doc["brightness"] = showController->getBrightness();

        // Show info
        doc["current_show"] = showController->getCurrentShowName();
        doc["auto_cycle"] = showController->isAutoCycleEnabled();

        // Network info
        doc["wifi_connected"] = (WiFi.status() == WL_CONNECTED);
        if (WiFi.status() == WL_CONNECTED) {
            doc["ip_address"] = WiFi.localIP().toString();
        }

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // GET /api/shows - List available shows
    server.on("/api/shows", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (showFactory == nullptr) {
            request->send(500, "application/json", "{\"error\":\"Factory not initialized\"}");
            return;
        }

        StaticJsonDocument<1024> doc;
        JsonArray shows = doc.createNestedArray("shows");

        const std::vector<ShowFactory::ShowInfo>& showList = showFactory->listShows();
        for (const auto& showInfo : showList) {
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
        [](AsyncWebServerRequest *request) {},
        NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            if (index == 0 && showController != nullptr) {
                StaticJsonDocument<256> doc;
                DeserializationError error = deserializeJson(doc, data, len);

                if (error) {
                    request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
                    return;
                }

                const char* showName = doc["name"];
                if (showName == nullptr) {
                    request->send(400, "application/json", "{\"success\":false,\"error\":\"Show name required\"}");
                    return;
                }

                if (showController->queueShowChange(showName)) {
                    request->send(200, "application/json", "{\"success\":true}");
                } else {
                    request->send(503, "application/json", "{\"success\":false,\"error\":\"Queue full\"}");
                }
            }
        }
    );

    // POST /api/brightness - Change brightness
    server.on("/api/brightness", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            if (index == 0 && showController != nullptr) {
                StaticJsonDocument<256> doc;
                DeserializationError error = deserializeJson(doc, data, len);

                if (error) {
                    request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
                    return;
                }

                if (!doc.containsKey("value")) {
                    request->send(400, "application/json", "{\"success\":false,\"error\":\"Brightness value required\"}");
                    return;
                }

                uint8_t brightness = doc["value"];
                if (showController->queueBrightnessChange(brightness)) {
                    request->send(200, "application/json", "{\"success\":true}");
                } else {
                    request->send(503, "application/json", "{\"success\":false,\"error\":\"Queue full\"}");
                }
            }
        }
    );

    // POST /api/auto-cycle - Toggle auto-cycle
    server.on("/api/auto-cycle", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            if (index == 0 && showController != nullptr) {
                StaticJsonDocument<256> doc;
                DeserializationError error = deserializeJson(doc, data, len);

                if (error) {
                    request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
                    return;
                }

                if (!doc.containsKey("enabled")) {
                    request->send(400, "application/json", "{\"success\":false,\"error\":\"Enabled field required\"}");
                    return;
                }

                bool enabled = doc["enabled"];
                if (showController->queueAutoCycleToggle(enabled)) {
                    request->send(200, "application/json", "{\"success\":true}");
                } else {
                    request->send(503, "application/json", "{\"success\":false,\"error\":\"Queue full\"}");
                }
            }
        }
    );
#endif
}

#ifdef ARDUINO
void WebServerManager::handleWiFiConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
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
        const char* ssid = doc["ssid"];
        const char* password = doc["password"];

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

        // Send success response
        request->send(200, "application/json", "{\"success\":true}");

        // Note: The Network task will detect config.isConfigured() and restart the device
    }
}
#endif

void WebServerManager::begin() {
#ifdef ARDUINO
    Serial.println("Starting webserver...");

    // Setup routes
    setupConfigRoutes();
    setupAPIRoutes();

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
