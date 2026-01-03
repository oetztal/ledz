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
        .params-section {
            background-color: #f8f9fa;
            border-radius: 10px;
            padding: 20px;
            margin-top: 15px;
            display: none;
        }
        .params-section.visible {
            display: block;
        }
        .param-row {
            margin-bottom: 15px;
        }
        .param-row:last-child {
            margin-bottom: 0;
        }
        .param-label {
            display: block;
            font-size: 13px;
            font-weight: 600;
            color: #495057;
            margin-bottom: 8px;
        }
        input[type="color"] {
            width: 100%;
            height: 50px;
            border: 2px solid #e9ecef;
            border-radius: 8px;
            cursor: pointer;
            padding: 5px;
        }
        input[type="number"] {
            width: 100%;
            padding: 12px;
            border: 2px solid #e9ecef;
            border-radius: 8px;
            font-size: 16px;
            transition: border-color 0.3s;
        }
        input[type="number"]:focus {
            outline: none;
            border-color: #667eea;
        }
        .apply-button {
            width: 100%;
            padding: 12px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            transition: transform 0.2s, box-shadow 0.2s;
            margin-top: 10px;
        }
        .apply-button:hover {
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(102, 126, 234, 0.4);
        }
        .apply-button:active {
            transform: translateY(0);
        }
        .about-link {
            display: block;
            text-align: center;
            padding: 15px;
            color: #667eea;
            text-decoration: none;
            font-size: 14px;
            font-weight: 600;
            border-top: 1px solid #e9ecef;
        }
        .about-link:hover {
            background-color: #f8f9fa;
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

                <!-- Parameter controls (shown based on selected show) -->
                <div id="solidParams" class="params-section">
                    <div class="param-row">
                        <label class="param-label" for="solidColor">Color</label>
                        <input type="color" id="solidColor" value="#ffffff">
                    </div>
                    <button class="apply-button" onclick="applySolidColor()">Apply Color</button>
                </div>

                <div id="mandelbrotParams" class="params-section">
                    <div class="param-row">
                        <label class="param-label" for="mandelbrotCre0">Cre0 (Real min)</label>
                        <input type="number" id="mandelbrotCre0" step="0.01" value="-1.05">
                    </div>
                    <div class="param-row">
                        <label class="param-label" for="mandelbrotCim0">Cim0 (Imaginary min)</label>
                        <input type="number" id="mandelbrotCim0" step="0.01" value="-0.3616">
                    </div>
                    <div class="param-row">
                        <label class="param-label" for="mandelbrotCim1">Cim1 (Imaginary max)</label>
                        <input type="number" id="mandelbrotCim1" step="0.01" value="-0.3156">
                    </div>
                    <div class="param-row">
                        <label class="param-label" for="mandelbrotScale">Scale</label>
                        <input type="number" id="mandelbrotScale" min="1" max="20" value="5">
                    </div>
                    <div class="param-row">
                        <label class="param-label" for="mandelbrotMaxIter">Max Iterations</label>
                        <input type="number" id="mandelbrotMaxIter" min="10" max="200" value="50">
                    </div>
                    <div class="param-row">
                        <label class="param-label" for="mandelbrotColorScale">Color Scale</label>
                        <input type="number" id="mandelbrotColorScale" min="1" max="50" value="10">
                    </div>
                    <button class="apply-button" onclick="applyMandelbrotParams()">Apply Parameters</button>
                </div>

                <div id="chaosParams" class="params-section">
                    <div class="param-row">
                        <label class="param-label" for="chaosRmin">Rmin (Start)</label>
                        <input type="number" id="chaosRmin" step="0.01" value="2.95">
                    </div>
                    <div class="param-row">
                        <label class="param-label" for="chaosRmax">Rmax (Maximum)</label>
                        <input type="number" id="chaosRmax" step="0.01" value="4.0">
                    </div>
                    <div class="param-row">
                        <label class="param-label" for="chaosRdelta">Rdelta (Increment)</label>
                        <input type="number" id="chaosRdelta" step="0.0001" value="0.0002">
                    </div>
                    <button class="apply-button" onclick="applyChaosParams()">Apply Parameters</button>
                </div>
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

            <div class="control-group">
                <label class="control-label">Strip Layout</label>
                <select id="layoutMode" style="margin-bottom: 10px;">
                    <option value="normal">Normal</option>
                    <option value="reverse">Reversed</option>
                    <option value="mirror">Mirrored</option>
                    <option value="reverse_mirror">Reversed + Mirrored</option>
                </select>
                <label class="control-label" for="deadLeds" style="margin-top: 15px;">Dead LEDs</label>
                <input type="number" id="deadLeds" min="0" max="100" value="0" style="width: 100%; padding: 12px; border: 2px solid #e9ecef; border-radius: 8px; font-size: 16px;">
                <button class="apply-button" onclick="applyLayout()" style="margin-top: 15px;">Apply Layout</button>
            </div>
        </div>

        <a href="/about" class="about-link">Device Information</a>
    </div>

    <script>
        let shows = [];
        let currentStatus = {};
        let pendingParameterConfig = false;  // Track if user is configuring parameters

        // Show/hide parameter sections based on selected show
        function updateParameterVisibility(showName) {
            document.getElementById('solidParams').classList.remove('visible');
            document.getElementById('mandelbrotParams').classList.remove('visible');
            document.getElementById('chaosParams').classList.remove('visible');

            if (showName === 'Solid') {
                document.getElementById('solidParams').classList.add('visible');
            } else if (showName === 'Mandelbrot') {
                document.getElementById('mandelbrotParams').classList.add('visible');
            } else if (showName === 'Chaos') {
                document.getElementById('chaosParams').classList.add('visible');
            }
        }

        // Apply Solid color parameters
        async function applySolidColor() {
            const hex = document.getElementById('solidColor').value;
            const r = parseInt(hex.substr(1,2), 16);
            const g = parseInt(hex.substr(3,2), 16);
            const b = parseInt(hex.substr(5,2), 16);

            try {
                await fetch('/api/show', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        name: 'Solid',
                        params: { r, g, b }
                    })
                });
                pendingParameterConfig = false;  // Applied successfully
            } catch (error) {
                console.error('Failed to apply color:', error);
            }
        }

        // Apply Mandelbrot parameters
        async function applyMandelbrotParams() {
            const Cre0 = parseFloat(document.getElementById('mandelbrotCre0').value);
            const Cim0 = parseFloat(document.getElementById('mandelbrotCim0').value);
            const Cim1 = parseFloat(document.getElementById('mandelbrotCim1').value);
            const scale = parseInt(document.getElementById('mandelbrotScale').value);
            const max_iterations = parseInt(document.getElementById('mandelbrotMaxIter').value);
            const color_scale = parseInt(document.getElementById('mandelbrotColorScale').value);

            try {
                await fetch('/api/show', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        name: 'Mandelbrot',
                        params: { Cre0, Cim0, Cim1, scale, max_iterations, color_scale }
                    })
                });
                pendingParameterConfig = false;  // Applied successfully
            } catch (error) {
                console.error('Failed to apply Mandelbrot parameters:', error);
            }
        }

        // Apply Chaos parameters
        async function applyChaosParams() {
            const Rmin = parseFloat(document.getElementById('chaosRmin').value);
            const Rmax = parseFloat(document.getElementById('chaosRmax').value);
            const Rdelta = parseFloat(document.getElementById('chaosRdelta').value);

            try {
                await fetch('/api/show', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        name: 'Chaos',
                        params: { Rmin, Rmax, Rdelta }
                    })
                });
                pendingParameterConfig = false;  // Applied successfully
            } catch (error) {
                console.error('Failed to apply Chaos parameters:', error);
            }
        }

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

                // Update description and show/hide parameters on change
                select.addEventListener('change', () => {
                    const selectedShow = shows.find(s => s.name === select.value);
                    document.getElementById('showDescription').textContent =
                        selectedShow ? selectedShow.description : '';
                    updateParameterVisibility(select.value);
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
                // Don't override dropdown if user is configuring parameters
                const showSelect = document.getElementById('showSelect');
                if (!pendingParameterConfig && showSelect.value !== currentStatus.current_show) {
                    showSelect.value = currentStatus.current_show;
                    const selectedShow = shows.find(s => s.name === currentStatus.current_show);
                    document.getElementById('showDescription').textContent =
                        selectedShow ? selectedShow.description : '';
                    updateParameterVisibility(currentStatus.current_show);
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

            // Don't auto-apply for shows with parameters - wait for user to click Apply button
            if (showName === 'Solid' || showName === 'Mandelbrot' || showName === 'Chaos') {
                pendingParameterConfig = true;  // User is now configuring parameters
                return;
            }

            // User selected a show without parameters
            pendingParameterConfig = false;

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

        // Load layout configuration
        async function loadLayout() {
            try {
                const response = await fetch('/api/layout');
                const layout = await response.json();

                // Set layout mode dropdown
                let mode = 'normal';
                if (layout.reverse && layout.mirror) mode = 'reverse_mirror';
                else if (layout.reverse) mode = 'reverse';
                else if (layout.mirror) mode = 'mirror';
                document.getElementById('layoutMode').value = mode;

                // Set dead LEDs
                document.getElementById('deadLeds').value = layout.dead_leds;
            } catch (error) {
                console.error('Failed to load layout:', error);
            }
        }

        // Apply layout configuration
        async function applyLayout() {
            const mode = document.getElementById('layoutMode').value;
            const dead_leds = parseInt(document.getElementById('deadLeds').value);

            let reverse = false;
            let mirror = false;
            if (mode === 'reverse') reverse = true;
            else if (mode === 'mirror') mirror = true;
            else if (mode === 'reverse_mirror') {
                reverse = true;
                mirror = true;
            }

            try {
                await fetch('/api/layout', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ reverse, mirror, dead_leds })
                });
                // Layout is applied immediately, no restart needed
            } catch (error) {
                console.error('Failed to apply layout:', error);
            }
        }

        // Initialize
        loadShows();
        updateStatus();
        loadLayout();

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
                StaticJsonDocument<512> doc;
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

                // Get parameters if provided
                String paramsJson;
                if (doc.containsKey("params")) {
                    JsonObject params = doc["params"];
                    serializeJson(params, paramsJson);
                } else {
                    paramsJson = "{}";
                }

                if (showController->queueShowChange(showName, paramsJson.c_str())) {
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

    // POST /api/layout - Change strip layout configuration
    server.on("/api/layout", HTTP_POST,
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
                if (showController->queueLayoutChange(layoutConfig.reverse, layoutConfig.mirror, layoutConfig.dead_leds)) {
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

    // GET /api/about - Device information
    server.on("/api/about", HTTP_GET, [this](AsyncWebServerRequest *request) {
        StaticJsonDocument<1024> doc;

        // Device info
        Config::DeviceConfig deviceConfig = config.loadDeviceConfig();
        doc["device_id"] = deviceConfig.device_id;

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

    // GET /about - About page
    server.on("/about", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>About - LED Controller</title>
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
