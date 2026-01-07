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
#endif

// Simple WiFi configuration HTML page (embedded)
const char CONFIG_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ledz - WiFi Setup</title>
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
            margin-bottom: 30px;
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
        <h1>ledz LED controller</h1>
        <h2 style="text-align: center; color: #666; font-size: 18px;">WiFi Configuration</h2>

        <div id="status" class="status"></div>

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
                    const hostname = result.hostname || 'the device';
                    statusDiv.innerHTML = `WiFi configured successfully!<br><br>The device will restart and connect to your network.<br><br>After restart, access it at: <strong><a href="http://${hostname}/" target="_blank">http://${hostname}/</a></strong>`;
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
    <title>ledz LED controller</title>
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

        .preset-button {
            padding: 8px 12px;
            margin: 4px;
            border: 1px solid #ddd;
            border-radius: 4px;
            background: #f5f5f5;
            cursor: pointer;
            font-size: 14px;
        }

        .preset-button:hover {
            background: #e0e0e0;
        }

        .small-button {
            padding: 6px 10px;
            margin: 4px;
            border: 1px solid #ccc;
            border-radius: 4px;
            background: #fff;
            cursor: pointer;
            font-size: 13px;
        }

        .small-button:hover {
            background: #f0f0f0;
        }

        input[type="text"] {
            width: 100%;
            padding: 10px;
            border: 2px solid #e9ecef;
            border-radius: 8px;
            font-size: 14px;
        }

        input[type="text"]:focus {
            outline: none;
            border-color: #667eea;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1 id="pageTitle">ledz</h1>
            <div class="device-id">
                <span id="deviceId">Loading...</span>
                <span id="firmwareVersion" style="opacity: 0.7; font-size: 0.9em; margin-left: 10px;"></span>
                <span id="otaPartition" style="opacity: 0.6; font-size: 0.85em; margin-left: 5px;"></span>
            </div>
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

                <div id="twoColorBlendParams" class="params-section">
                    <div class="param-row">
                        <label class="param-label" for="twoColorBlendColor1">Start Color</label>
                        <input type="color" id="twoColorBlendColor1" value="#ff0000">
                    </div>
                    <div class="param-row">
                        <label class="param-label" for="twoColorBlendColor2">End Color</label>
                        <input type="color" id="twoColorBlendColor2" value="#0000ff">
                    </div>
                    <button class="apply-button" onclick="applyTwoColorBlendParams()">Apply Colors</button>
                </div>

                <div id="colorRangesParams" class="params-section">
                    <small style="display:block; margin-bottom:8px; color:#666;">
                        Single color with smooth fade-in (default: warm white), or use multiple colors for patterns/flags.
                    </small>
                    <div style="margin-bottom: 12px;">
                        <strong>Presets:</strong>
                        <button class="preset-button" onclick="loadWarmWhite()">💡 Warm White</button>
                        <button class="preset-button" onclick="loadUkraineFlag()">🇺🇦 Ukraine</button>
                        <button class="preset-button" onclick="loadItalianFlag()">🇮🇹 Italy</button>
                        <button class="preset-button" onclick="loadFrenchFlag()">🇫🇷 France</button>
                    </div>
                    <div id="colorRangesColorInputs">
                        <div class="param-row">
                            <label class="param-label" for="colorRangesColor1">Color 1</label>
                            <input type="color" id="colorRangesColor1" value="#fffae6">
                        </div>
                    </div>
                    <div class="param-row">
                        <button class="small-button" onclick="addColorRangesColor()">+ Add Color</button>
                        <button class="small-button" onclick="removeColorRangesColor()">- Remove Color</button>
                    </div>
                    <div class="param-row">
                        <label class="param-label" for="colorRangesRanges">Ranges (%) [optional]</label>
                        <input type="text" id="colorRangesRanges" placeholder="e.g., 33.3, 66.6" value="">
                        <small style="display:block; margin-top:4px; color:#666;">
                            Leave empty for equal distribution.<br>
                            For N colors, enter N-1 boundary percentages:<br>
                            • 2 colors = 1 value (e.g., "50" or "60")<br>
                            • 3 colors = 2 values (e.g., "33.3, 66.6" or "25, 75")<br>
                            • 4 colors = 3 values (e.g., "25, 50, 75")
                        </small>
                    </div>
                    <button class="apply-button" onclick="applyColorRangesParams()">Apply Pattern</button>
                </div>

                <div id="starlightParams" class="params-section">
                    <div class="param-row">
                        <label class="param-label" for="starlightProbability">Spawn Probability (0.0-1.0)</label>
                        <input type="number" id="starlightProbability" step="0.01" min="0" max="1" value="0.01">
                        <small style="display:block; margin-top:4px; color:#666;">Higher = more stars spawn per frame</small>
                    </div>
                    <div class="param-row">
                        <label class="param-label" for="starlightLength">Hold Time (ms)</label>
                        <input type="number" id="starlightLength" step="100" min="100" max="30000" value="5000">
                        <small style="display:block; margin-top:4px; color:#666;">Duration at full brightness</small>
                    </div>
                    <div class="param-row">
                        <label class="param-label" for="starlightFade">Fade Time (ms)</label>
                        <input type="number" id="starlightFade" step="100" min="100" max="10000" value="1000">
                        <small style="display:block; margin-top:4px; color:#666;">Fade-in and fade-out duration</small>
                    </div>
                    <div class="param-row">
                        <label class="param-label" for="starlightColor">Star Color</label>
                        <input type="color" id="starlightColor" value="#ffb432">
                        <small style="display:block; margin-top:4px; color:#666;">Default: warm white (255, 180, 50)</small>
                    </div>
                    <button class="apply-button" onclick="applyStarlightParams()">Apply Parameters</button>
                </div>

                <div id="waveParams" class="params-section">
                    <div class="param-row">
                        <label class="param-label" for="waveSpeed">Wave Speed</label>
                        <input type="number" id="waveSpeed" step="0.1" min="0.1" max="10" value="1.0">
                        <small style="display:block; margin-top:4px; color:#666;">Higher = faster wave propagation</small>
                    </div>
                    <div class="param-row">
                        <label class="param-label" for="waveDecay">Decay Rate</label>
                        <input type="number" id="waveDecay" step="0.1" min="0" max="10" value="2.0">
                        <small style="display:block; margin-top:4px; color:#666;">Higher = faster brightness fade towards ends</small>
                    </div>
                    <div class="param-row">
                        <label class="param-label" for="waveBrightnessFreq">Brightness Frequency</label>
                        <input type="number" id="waveBrightnessFreq" step="0.01" min="0.01" max="1" value="0.1">
                        <small style="display:block; margin-top:4px; color:#666;">Frequency of source brightness pulsation</small>
                    </div>
                    <div class="param-row">
                        <label class="param-label" for="waveWavelength">Wavelength</label>
                        <input type="number" id="waveWavelength" step="0.5" min="1" max="20" value="6.0">
                        <small style="display:block; margin-top:4px; color:#666;">Higher = longer, more spread out waves</small>
                    </div>
                    <button class="apply-button" onclick="applyWaveParams()">Apply Parameters</button>
                </div>

                <div id="morseCodeParams" class="params-section">
                    <div class="param-row">
                        <label class="param-label" for="morseMessage">Message</label>
                        <input type="text" id="morseMessage" value="HELLO WORLD!" style="width:100%; padding:8px;">
                        <small style="display:block; margin-top:4px; color:#666;">Text to encode in Morse code (letters, numbers, basic punctuation)</small>
                    </div>
                    <div class="param-row">
                        <label class="param-label" for="morseSpeed">Scroll Speed</label>
                        <input type="number" id="morseSpeed" step="0.1" min="0.1" max="5" value="0.2">
                        <small style="display:block; margin-top:4px; color:#666;">Higher = faster scrolling (LEDs per frame)</small>
                    </div>
                    <div class="param-row">
                        <label class="param-label" for="morseDotLength">Dot Length</label>
                        <input type="number" id="morseDotLength" step="1" min="1" max="10" value="1">
                        <small style="display:block; margin-top:4px; color:#666;">Number of LEDs per dot</small>
                    </div>
                    <div class="param-row">
                        <label class="param-label" for="morseDashLength">Dash Length</label>
                        <input type="number" id="morseDashLength" step="1" min="1" max="20" value="3">
                        <small style="display:block; margin-top:4px; color:#666;">Number of LEDs per dash (typically 2-3x dot length)</small>
                    </div>
                    <div class="param-row">
                        <label class="param-label" for="morseSymbolSpace">Symbol Space</label>
                        <input type="number" id="morseSymbolSpace" step="1" min="1" max="10" value="2">
                        <small style="display:block; margin-top:4px; color:#666;">Dark LEDs between dots/dashes within letters</small>
                    </div>
                    <div class="param-row">
                        <label class="param-label" for="morseLetterSpace">Letter Space</label>
                        <input type="number" id="morseLetterSpace" step="1" min="1" max="10" value="3">
                        <small style="display:block; margin-top:4px; color:#666;">Dark LEDs between letters</small>
                    </div>
                    <div class="param-row">
                        <label class="param-label" for="morseWordSpace">Word Space</label>
                        <input type="number" id="morseWordSpace" step="1" min="1" max="20" value="5">
                        <small style="display:block; margin-top:4px; color:#666;">Dark LEDs between words</small>
                    </div>
                    <button class="apply-button" onclick="applyMorseCodeParams()">Apply Parameters</button>
                </div>

                <div id="theaterChaseParams" class="params-section">
                    <div class="param-row">
                        <label class="param-label" for="theaterStepsPerCycle">Steps per Color Cycle</label>
                        <input type="number" id="theaterStepsPerCycle" step="7" min="7" max="140" value="21">
                        <small style="display:block; margin-top:4px; color:#666;">Number of steps for one complete rainbow cycle (multiple of 7 recommended)</small>
                    </div>
                    <button class="apply-button" onclick="applyTheaterChaseParams()">Apply Parameters</button>
                </div>

                <div id="stroboscopeParams" class="params-section">
                    <div class="param-row">
                        <label class="param-label" for="stroboscopeColor">Flash Color</label>
                        <input type="color" id="stroboscopeColor" value="#ffffff">
                        <small style="display:block; margin-top:4px; color:#666;">Color to flash (default: white)</small>
                    </div>
                    <div class="param-row">
                        <label class="param-label" for="stroboscopeOnCycles">On Cycles</label>
                        <input type="number" id="stroboscopeOnCycles" step="1" min="1" max="20" value="1">
                        <small style="display:block; margin-top:4px; color:#666;">Number of frames to flash the color</small>
                    </div>
                    <div class="param-row">
                        <label class="param-label" for="stroboscopeOffCycles">Off Cycles</label>
                        <input type="number" id="stroboscopeOffCycles" step="1" min="1" max="100" value="10">
                        <small style="display:block; margin-top:4px; color:#666;">Number of frames to stay black</small>
                    </div>
                    <button class="apply-button" onclick="applyStroboscopeParams()">Apply Parameters</button>
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

        <div style="display: flex; gap: 10px; margin-top: 20px;">
            <a href="/settings" class="about-link" style="flex: 1; text-align: center;">Settings</a>
            <a href="/about" class="about-link" style="flex: 1; text-align: center;">Device Info</a>
        </div>
    </div>

    <script>
        let shows = [];
        let currentStatus = {};
        let pendingParameterConfig = false;  // Track if user is configuring parameters

        // Show/hide parameter sections based on selected show
        function updateParameterVisibility(showName) {
            document.getElementById('mandelbrotParams').classList.remove('visible');
            document.getElementById('chaosParams').classList.remove('visible');
            document.getElementById('twoColorBlendParams').classList.remove('visible');
            document.getElementById('colorRangesParams').classList.remove('visible');
            document.getElementById('starlightParams').classList.remove('visible');
            document.getElementById('waveParams').classList.remove('visible');
            document.getElementById('morseCodeParams').classList.remove('visible');
            document.getElementById('theaterChaseParams').classList.remove('visible');
            document.getElementById('stroboscopeParams').classList.remove('visible');

            if (showName === 'Mandelbrot') {
                document.getElementById('mandelbrotParams').classList.add('visible');
            } else if (showName === 'Chaos') {
                document.getElementById('chaosParams').classList.add('visible');
            } else if (showName === 'TwoColorBlend') {
                document.getElementById('twoColorBlendParams').classList.add('visible');
            } else if (showName === 'Solid') {
                document.getElementById('colorRangesParams').classList.add('visible');
            } else if (showName === 'Starlight') {
                document.getElementById('starlightParams').classList.add('visible');
            } else if (showName === 'Wave') {
                document.getElementById('waveParams').classList.add('visible');
            } else if (showName === 'MorseCode') {
                document.getElementById('morseCodeParams').classList.add('visible');
            } else if (showName === 'TheaterChase') {
                document.getElementById('theaterChaseParams').classList.add('visible');
            } else if (showName === 'Stroboscope') {
                document.getElementById('stroboscopeParams').classList.add('visible');
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

        // Apply TwoColorBlend parameters
        async function applyTwoColorBlendParams() {
            const hex1 = document.getElementById('twoColorBlendColor1').value;
            const r1 = parseInt(hex1.substr(1,2), 16);
            const g1 = parseInt(hex1.substr(3,2), 16);
            const b1 = parseInt(hex1.substr(5,2), 16);

            const hex2 = document.getElementById('twoColorBlendColor2').value;
            const r2 = parseInt(hex2.substr(1,2), 16);
            const g2 = parseInt(hex2.substr(3,2), 16);
            const b2 = parseInt(hex2.substr(5,2), 16);

            try {
                await fetch('/api/show', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        name: 'TwoColorBlend',
                        params: { r1, g1, b1, r2, g2, b2 }
                    })
                });
                pendingParameterConfig = false;  // Applied successfully
            } catch (error) {
                console.error('Failed to apply TwoColorBlend parameters:', error);
            }
        }

        // ColorRanges color management
        let colorRangesColorCount = 1;

        function addColorRangesColor() {
            colorRangesColorCount++;
            const container = document.getElementById('colorRangesColorInputs');
            const newColorDiv = document.createElement('div');
            newColorDiv.className = 'param-row';
            newColorDiv.innerHTML = `
                <label class="param-label" for="colorRangesColor${colorRangesColorCount}">Color ${colorRangesColorCount}</label>
                <input type="color" id="colorRangesColor${colorRangesColorCount}" value="#ffffff">
            `;
            container.appendChild(newColorDiv);
        }

        function removeColorRangesColor() {
            if (colorRangesColorCount > 1) {
                const container = document.getElementById('colorRangesColorInputs');
                if (container.children.length > 0) {
                    // Use lastElementChild to skip text nodes (whitespace)
                    const lastElement = container.lastElementChild;
                    if (lastElement) {
                        container.removeChild(lastElement);
                        colorRangesColorCount--;
                    }
                }
            }
        }

        // Load Warm White preset (single color)
        function loadWarmWhite() {
            // Reset to 1 color
            while (colorRangesColorCount > 1) {
                removeColorRangesColor();
            }
            document.getElementById('colorRangesColor1').value = '#fffae6'; // Warm white
            document.getElementById('colorRangesRanges').value = ''; // No ranges for single color
        }

        // Load Ukraine flag preset
        function loadUkraineFlag() {
            // Reset to 2 colors
            while (colorRangesColorCount > 2) {
                removeColorRangesColor();
            }
            document.getElementById('colorRangesColor1').value = '#0057b7'; // Blue
            document.getElementById('colorRangesColor2').value = '#ffd700'; // Yellow
            document.getElementById('colorRangesRanges').value = ''; // Equal distribution
        }

        // Load Italian flag preset
        function loadItalianFlag() {
            // Ensure we have 3 colors
            while (colorRangesColorCount < 3) {
                addColorRangesColor();
            }
            while (colorRangesColorCount > 3) {
                removeColorRangesColor();
            }
            document.getElementById('colorRangesColor1').value = '#008c45'; // Green
            document.getElementById('colorRangesColor2').value = '#ffffff'; // White
            document.getElementById('colorRangesColor3').value = '#cd212a'; // Red
            document.getElementById('colorRangesRanges').value = ''; // Equal distribution
        }

        // Load French flag preset
        function loadFrenchFlag() {
            // Ensure we have 3 colors
            while (colorRangesColorCount < 3) {
                addColorRangesColor();
            }
            while (colorRangesColorCount > 3) {
                removeColorRangesColor();
            }
            document.getElementById('colorRangesColor1').value = '#0055a4'; // Blue
            document.getElementById('colorRangesColor2').value = '#ffffff'; // White
            document.getElementById('colorRangesColor3').value = '#ef4135'; // Red
            document.getElementById('colorRangesRanges').value = ''; // Equal distribution
        }

        // Apply ColorRanges parameters
        async function applyColorRangesParams() {
            const colors = [];

            // Collect all color values (supports 1 or more colors)
            for (let i = 1; i <= colorRangesColorCount; i++) {
                const colorInput = document.getElementById(`colorRangesColor${i}`);
                if (colorInput) {
                    const hex = colorInput.value;
                    const r = parseInt(hex.substr(1,2), 16);
                    const g = parseInt(hex.substr(3,2), 16);
                    const b = parseInt(hex.substr(5,2), 16);
                    colors.push([r, g, b]);
                }
            }

            // Parse ranges (optional, only for multi-color mode)
            const rangesText = document.getElementById('colorRangesRanges').value.trim();
            let ranges = [];
            if (rangesText && colors.length > 1) {
                ranges = rangesText.split(',').map(s => parseFloat(s.trim())).filter(n => !isNaN(n));

                // Validate: should have colors.length - 1 ranges
                if (ranges.length > 0 && ranges.length !== colors.length - 1) {
                    alert(`Warning: For ${colors.length} colors, you need ${colors.length - 1} range value(s).\nYou provided ${ranges.length}. Using equal distribution instead.`);
                    ranges = []; // Clear invalid ranges
                }
            }

            // Always use colors array format
            const params = { colors };
            if (ranges.length > 0) {
                params.ranges = ranges;
            }

            console.log('Solid params:', JSON.stringify(params));

            try {
                await fetch('/api/show', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        name: 'Solid',
                        params: params
                    })
                });
                pendingParameterConfig = false;  // Applied successfully
            } catch (error) {
                console.error('Failed to apply Solid parameters:', error);
            }
        }

        // Apply Starlight parameters
        async function applyStarlightParams() {
            const probability = parseFloat(document.getElementById('starlightProbability').value);
            const length = parseInt(document.getElementById('starlightLength').value);
            const fade = parseInt(document.getElementById('starlightFade').value);

            const hex = document.getElementById('starlightColor').value;
            const r = parseInt(hex.substr(1,2), 16);
            const g = parseInt(hex.substr(3,2), 16);
            const b = parseInt(hex.substr(5,2), 16);

            try {
                await fetch('/api/show', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        name: 'Starlight',
                        params: { probability, length, fade, r, g, b }
                    })
                });
                pendingParameterConfig = false;  // Applied successfully
            } catch (error) {
                console.error('Failed to apply Starlight parameters:', error);
            }
        }

        // Apply Wave parameters
        async function applyWaveParams() {
            const wave_speed = parseFloat(document.getElementById('waveSpeed').value);
            const decay_rate = parseFloat(document.getElementById('waveDecay').value);
            const brightness_frequency = parseFloat(document.getElementById('waveBrightnessFreq').value);
            const wavelength = parseFloat(document.getElementById('waveWavelength').value);

            try {
                pendingParameterConfig = true;
                await fetch('/api/show', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        name: 'Wave',
                        params: { wave_speed, decay_rate, brightness_frequency, wavelength }
                    })
                });
                pendingParameterConfig = false;  // Applied successfully
            } catch (error) {
                console.error('Failed to apply Wave parameters:', error);
            }
        }

        // Apply MorseCode parameters
        async function applyMorseCodeParams() {
            const message = document.getElementById('morseMessage').value;
            const speed = parseFloat(document.getElementById('morseSpeed').value);
            const dot_length = parseInt(document.getElementById('morseDotLength').value);
            const dash_length = parseInt(document.getElementById('morseDashLength').value);
            const symbol_space = parseInt(document.getElementById('morseSymbolSpace').value);
            const letter_space = parseInt(document.getElementById('morseLetterSpace').value);
            const word_space = parseInt(document.getElementById('morseWordSpace').value);

            try {
                pendingParameterConfig = true;
                await fetch('/api/show', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        name: 'MorseCode',
                        params: { message, speed, dot_length, dash_length, symbol_space, letter_space, word_space }
                    })
                });
                pendingParameterConfig = false;  // Applied successfully
            } catch (error) {
                console.error('Failed to apply MorseCode parameters:', error);
            }
        }

        // Apply TheaterChase parameters
        async function applyTheaterChaseParams() {
            const num_steps_per_cycle = parseInt(document.getElementById('theaterStepsPerCycle').value);

            try {
                pendingParameterConfig = true;
                await fetch('/api/show', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        name: 'TheaterChase',
                        params: { num_steps_per_cycle }
                    })
                });
                pendingParameterConfig = false;  // Applied successfully
            } catch (error) {
                console.error('Failed to apply TheaterChase parameters:', error);
            }
        }

        // Apply Stroboscope parameters
        async function applyStroboscopeParams() {
            const hex = document.getElementById('stroboscopeColor').value;
            const r = parseInt(hex.substring(1, 3), 16);
            const g = parseInt(hex.substring(3, 5), 16);
            const b = parseInt(hex.substring(5, 7), 16);
            const on_cycles = parseInt(document.getElementById('stroboscopeOnCycles').value);
            const off_cycles = parseInt(document.getElementById('stroboscopeOffCycles').value);

            try {
                pendingParameterConfig = true;
                await fetch('/api/show', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        name: 'Stroboscope',
                        params: { r, g, b, on_cycles, off_cycles }
                    })
                });
                pendingParameterConfig = false;  // Applied successfully
            } catch (error) {
                console.error('Failed to apply Stroboscope parameters:', error);
            }
        }

        // Update device info display
        function updateDeviceInfo() {
            const pageTitleElement = document.getElementById('pageTitle');
            const deviceIdElement = document.getElementById('deviceId');

            if (currentStatus.device_id) {
                const hostname = "ledz-" + currentStatus.device_id.toLowerCase();
                const hasCustomName = currentStatus.device_name && currentStatus.device_name !== currentStatus.device_id;

                // Update page title with custom name if set
                if (hasCustomName) {
                    pageTitleElement.textContent = 'ledz ' + currentStatus.device_name;
                } else {
                    pageTitleElement.textContent = 'ledz';
                }

                // Device ID and technical details
                let infoHTML = currentStatus.device_id;

                if (currentStatus.wifi_connected && currentStatus.wifi_ssid) {
                    infoHTML += `<br><small style="font-size: 11px; font-weight: normal;">
                        WiFi: ${currentStatus.wifi_ssid}<br>
                        Access at: <a href="http://${hostname}.local/" style="color: inherit; text-decoration: underline;">${hostname}.local</a>
                    </small>`;
                } else {
                    infoHTML += `<br><small style="font-size: 11px; font-weight: normal;">
                        Access at: <a href="http://${hostname}.local/" style="color: inherit; text-decoration: underline;">${hostname}.local</a>
                    </small>`;
                }

                deviceIdElement.innerHTML = infoHTML;
            }

            // Update firmware version
            const firmwareVersionElement = document.getElementById('firmwareVersion');
            if (firmwareVersionElement && currentStatus.firmware_version) {
                firmwareVersionElement.textContent = currentStatus.firmware_version;
            }

            // Update OTA partition
            const otaPartitionElement = document.getElementById('otaPartition');
            if (otaPartitionElement && currentStatus.ota_partition) {
                otaPartitionElement.textContent = `(${currentStatus.ota_partition})`;
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

                // Update device info with mDNS hostname
                updateDeviceInfo();
                document.getElementById('currentShow').textContent = currentStatus.current_show;
                document.getElementById('currentBrightness').textContent = currentStatus.brightness;

                // Update UI controlst c without triggering change events
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
            const showsWithParams = ['Solid', 'Mandelbrot', 'Chaos', 'TwoColorBlend',
                                     'Starlight', 'Wave', 'MorseCode', 'TheaterChase', 'Stroboscope'];

            if (showsWithParams.includes(showName)) {
                pendingParameterConfig = true;  // User is now configuring parameters
                return;
            }

            // User selected a show without parameters (Rainbow, ColorRun, Jump)
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

        // Brightness change handler (instantaneous updates)
        document.getElementById('brightnessSlider').addEventListener('input', async (e) => {
            const value = e.target.value;
            document.getElementById('brightnessValue').textContent = value;

            try {
                await fetch('/api/brightness', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ value: parseInt(value) })
                });
            } catch (error) {
                console.error('Failed to change brightness:', error);
            }
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

        // Update status every minute
        setInterval(updateStatus, 10000);
    </script>
</body>
</html>
)rawliteral";

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
    ss << "[HTTP] " << request->client()->remoteIP().toString().c_str() << " " << request->url().c_str() << " " << request->methodToString();

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
        StaticJsonDocument<512> doc;

        // Device info
        Config::DeviceConfig deviceConfig = config.loadDeviceConfig();
        doc["device_id"] = deviceConfig.device_id;
        doc["device_name"] = deviceConfig.device_name;
        doc["brightness"] = showController.getBrightness();
        doc["firmware_version"] = FIRMWARE_VERSION;

        // OTA partition info
        const esp_partition_t* running_partition = esp_ota_get_running_partition();
        if (running_partition != nullptr) {
            doc["ota_partition"] = running_partition->label;
        }

        // Show info
        doc["current_show"] = showController.getCurrentShowName();
        doc["auto_cycle"] = showController.isAutoCycleEnabled();

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
    server.addMiddleware(&logging);

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
ConfigWebServerManager::ConfigWebServerManager(Config::ConfigManager &config, Network &network, ShowController &showController)
    : WebServerManager(config, network, showController) {
}

void ConfigWebServerManager::setupRoutes() {
#ifdef ARDUINO
    Serial.println("Setting up config-only routes...");

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
OperationalWebServerManager::OperationalWebServerManager(Config::ConfigManager &config, Network &network, ShowController &showController)
    : WebServerManager(config, network, showController) {
}

void OperationalWebServerManager::setupRoutes() {
#ifdef ARDUINO
    Serial.println("Setting up operational routes...");

    // Setup config routes (for reconfiguration access)
    setupConfigRoutes();

    // Setup API routes (LED control, status, etc.)
    setupAPIRoutes();

    // Add 404 handler
    server.onNotFound([](AsyncWebServerRequest *request) {
        Serial.printf("[HTTP] 404 Not Found: %s\n", request->url().c_str());
        request->send(404, "text/plain", "Not found");
    });
#endif
}
