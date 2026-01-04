# AGENTS.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**ledz** is an ESP32-based LED controller with web interface for WiFi configuration and show control. It runs on the Adafruit QT Py ESP32-S3 (no PSRAM) and controls WS2812B/NeoPixel LED strips.

**Key characteristics:**
- Dual-core architecture (Core 0: Network/Web, Core 1: LED rendering)
- Thread-safe communication via FreeRTOS queues
- Web-based configuration and control
- Persistent storage via ESP32 Preferences (NVS)
- mDNS support for easy discovery (e.g., `led-zaabbcc.local`)
- AP mode for initial WiFi setup with captive portal

## Build Commands

### Build for ESP32 (primary target)
```bash
pio run -e adafruit_qtpy_esp32s3_nopsram
```

### Build for native environment (tests only)
```bash
pio run -e native
```

### Run tests
```bash
# Run all tests
pio test -e native

# Run specific test suite
pio test -e native -f test_color
pio test -e native -f test_jump
pio test -e native -f test_smooth_blend

# Verbose output
pio test -e native -v
```

### Upload to device
```bash
pio run -e adafruit_qtpy_esp32s3_nopsram -t upload
```

### Monitor serial output
```bash
pio device monitor
```

## Architecture

### Dual-Core Threading Model

The ESP32's dual cores are utilized for concurrent operation:

- **Core 0 (Network Task)**: Runs `Network::task()` - handles WiFi, NTP, mDNS, and webserver
- **Core 1 (LED Task)**: Runs `ledShowTask()` - renders LED animations at ~100Hz (10ms cycle)

**Thread-safe communication**: Uses FreeRTOS queue (`ShowCommand`) to pass commands from webserver (Core 0) to LED task (Core 1). The LED task owns the show pointer and is the only one that modifies it.

### Critical Threading Rules

1. **WebServerManager** (Core 0) must NEVER directly modify shows or LED state - it queues commands via `ShowController::queueShowChange()`
2. **LED Task** (Core 1) processes queued commands via `ShowController::processCommands()` each iteration
3. The `showTaskHandle` can be suspended (e.g., for factory reset) to safely manipulate LED state
4. All ownership is managed via `std::unique_ptr` with C++ move semantics for transfers

### Component Relationships

**Ownership hierarchy** (via `std::unique_ptr` and move semantics):
```
main.cpp (setup)
├─> ShowController (owns: currentShow, baseStrip, layout)
│   ├─> ShowFactory& (reference, creates shows via unique_ptr)
│   └─> Config& (reference, persistence to NVS)
├─> Network (owns: webServer via unique_ptr)
│   ├─> Status& (reference)
│   ├─> Config& (reference)
│   └─> WebServerManager (owned by Network, receives via std::move)
│       ├─> Config& (reference)
│       ├─> Network& (reference)
│       └─> ShowController& (reference, queues show changes)
```

**Strip hierarchy** (ShowController owns both):
- `baseStrip` (unique_ptr): Wraps Adafruit_NeoPixel hardware
- `layout` (unique_ptr): Decorates baseStrip with transformations

### Show System

**Show inheritance**: All shows inherit from `Show::Show` base class with virtual `execute(Strip::Strip&, Iteration)` method.

**Show creation flow**:
1. User configures parameters in web UI (or API call with JSON)
2. `WebServerManager` receives POST to `/api/show` with `{name, params}`
3. `ShowController::queueShowChange()` adds command to FreeRTOS queue
4. LED task calls `ShowController::processCommands()`
5. `ShowFactory::createShow()` parses JSON and returns `std::unique_ptr<Show::Show>&&` (rvalue reference)
6. ShowController receives ownership via move: `currentShow = std::move(newShow)`
7. Parameters saved to NVS for persistence across reboots

**Adding a new show**:
1. Create `src/show/MyShow.h` and `src/show/MyShow.cpp` inheriting from `Show::Show`
2. Register in `ShowFactory.cpp` constructor with lambda factory function
3. Add JSON parameter parsing in `ShowFactory::createShow()` (optional)
4. Add UI controls in `WebServerManager.cpp` (optional)
5. Add to `updateParameterVisibility()` function if it has parameters
6. Include header in `main.cpp`

### Strip Abstraction

**Base**: Wraps `Adafruit_NeoPixel`, handles hardware communication
**Layout**: Decorates Base with transformations (reverse, mirror, dead LED masking)

**Initialization flow**:
1. main.cpp creates baseStrip: `auto base = std::unique_ptr<Strip::Base>(new Strip::Base(...))`
2. Transfers ownership to ShowController: `showController.setStrip(std::move(base))`
3. ShowController creates layout internally: `layout.reset(new Strip::Layout(*baseStrip, ...))`

**Runtime reconfiguration**: When layout settings change, ShowController recreates layout internally using `layout.reset(new Strip::Layout(*baseStrip, ...))` with updated parameters.

### Configuration Persistence

Uses ESP32 Preferences API (NVS) with namespace "ledctrl":

- **WiFiConfig**: SSID, password, configured flag, connection failure counter
- **ShowConfig**: current_show name, params_json, auto_cycle settings
- **DeviceConfig**: brightness, device_name, device_id, num_pixels
- **LayoutConfig**: reverse, mirror, dead_leds

Access via `Config::ConfigManager` singleton. Always call `config.begin()` in setup before use.

### Network Modes

**AP Mode** (Access Point):
- Starts on first boot or after 3 connection failures
- SSID format: `ledz-AABBCC` (from MAC address last 3 bytes)
- IP: 192.168.4.1
- Captive portal redirects all DNS queries to device
- Restarts automatically after WiFi configuration received

**STA Mode** (Station/Client):
- Connects to configured WiFi network
- Advertises via mDNS as `ledzaabbcc.local`
- Updates NTP time every 300 seconds
- Auto-reconnects if connection lost
- Increments failure counter on failed connection (triggers AP mode after 3 failures)

### WebServerManager Structure

**Embedded HTML**: The entire web interface is embedded as C++ string literals in `WebServerManager.cpp` using raw string literals. This eliminates filesystem dependencies.

**Key endpoints**:
- `GET /` - Main control interface
- `GET /config` - WiFi configuration page (AP mode)
- `POST /api/show` - Change show with JSON parameters
- `POST /api/brightness` - Set brightness (0-255)
- `GET /api/status` - Device status and current show
- `POST /api/settings/factory-reset` - Factory reset (clears LEDs, suspends LED task, erases NVS)

**Adding new parameters to a show's UI**:
1. Add HTML `<div id="showNameParams" class="params-section">` with inputs
2. Add case to `updateParameterVisibility()` to show/hide section
3. Add `applyShowNameParams()` JavaScript function to collect values and POST to `/api/show`

## Important Patterns & Conventions

### Memory Management

**The project uses zero raw pointers for resource ownership.** All dynamic memory is managed via `std::unique_ptr` with C++ move semantics for ownership transfer.

**Ownership rules**:
- Use `std::unique_ptr<T>` for owned resources
- Transfer ownership with `std::move()`: `owner = std::move(resource)`
- Non-owning access uses references: `T&` (never raw pointers)
- Factory functions return rvalue references: `std::unique_ptr<T>&&`

**Examples**:
```cpp
// Creating owned resource
auto base = std::unique_ptr<Strip::Base>(new Strip::Base(pin, num_pixels));

// Transferring ownership
showController.setStrip(std::move(base));  // base is now nullptr

// Factory returns unique_ptr via rvalue reference
std::unique_ptr<Show::Show> newShow = factory.createShow(name, params);
currentShow = std::move(newShow);

// Non-owning access via reference
WebServerManager(Config& config, Network& network, ShowController& controller);
```

**When adding new code**:
- NEVER use `new` without immediately wrapping in `std::unique_ptr`
- NEVER store raw pointers to owned resources
- Use references (`&`) for non-owning access, never raw pointers
- Pass `std::unique_ptr` by rvalue reference (`&&`) or via `std::move()`

### ShowFactory Pattern

ShowFactory uses lambda functions that return `std::unique_ptr<Show::Show>&&` (rvalue reference):

```cpp
// In ShowFactory.cpp constructor
registerShow("Solid", "Solid color", []() {
    return std::make_unique<Show::Solid>(255, 255, 255);
});

// Factory method signature
std::unique_ptr<Show::Show>&& createShow(const char* name, const char* paramsJson);

// Usage in ShowController
std::unique_ptr<Show::Show> newShow = factory.createShow(name, params);
if (newShow) {
    currentShow = std::move(newShow);  // Transfer ownership
}
```

The rvalue reference return type enables move semantics and prevents accidental copies.

### Show Parameter Defaults
When adding JSON parsing in ShowFactory, always use the `|` operator for defaults:
```cpp
uint8_t r = doc["r"] | 255;  // Defaults to 255 if not present
float speed = doc["speed"] | 1.0f;
```

### Factory Reset Sequence
Must suspend LED task BEFORE clearing strip to prevent race condition:
```cpp
vTaskSuspend(showTaskHandle);
showController->clearStrip();
delay(500);  // Visible feedback
config.reset();
ESP.restart();
```

### Show Selection UI Behavior
Shows with parameters should NOT auto-start when selected from dropdown - they must wait for user to click "Apply Parameters". Add show name to `showsWithParams` array in the change event handler.

### Serial Logging
Use `Serial.printf()` for formatted debug output. Key points to log:
- Show creation with parameters
- Network state changes (AP/STA mode)
- Configuration changes
- LED task statistics (every 10 seconds)

## File Locations

**Core components**: `src/` (main.cpp, Config, Network, ShowController, ShowFactory, WebServerManager)
**Shows**: `src/show/` (each show is separate .h/.cpp pair)
**Strip abstraction**: `src/strip/` (Strip.h, Base, Layout)
**Utilities**: `src/support/` (SmoothBlend for color transitions)
**Tests**: `test/test_*/` (each subdirectory is independent test suite)
**Documentation**: `docs/` (SHOW_PARAMETERS.md, MDNS.md)

## Platform-Specific Code

Use `#ifdef ARDUINO` to guard ESP32-specific code:
```cpp
#ifdef ARDUINO
    WiFi.begin(ssid, password);
    vTaskDelay(500 / portTICK_PERIOD_MS);
#endif
```

Native environment is for testing only - it builds shows, color utilities, and strip abstractions but not network/webserver code.

## Common Modifications

### Adding a flag preset to ColorRanges
1. Add button in HTML: `<button class="preset-button" onclick="loadMyFlag()">🇫🇷 My Flag</button>`
2. Add JavaScript function:
```javascript
function loadMyFlag() {
    while (colorRangesColorCount < 3) addColorRangesColor();
    while (colorRangesColorCount > 3) removeColorRangesColor();
    document.getElementById('colorRangesColor1').value = '#0055a4';  // Blue
    document.getElementById('colorRangesColor2').value = '#ffffff';  // White
    document.getElementById('colorRangesColor3').value = '#ef4135';  // Red
    document.getElementById('colorRangesRanges').value = '';  // Equal distribution
}
```

### Changing show order in dropdown
Modify registration order in `ShowFactory.cpp` constructor. Note: Default show is set in `ShowController.cpp` line 47 and is independent of dropdown order.

### Adding new device settings
1. Add field to appropriate struct in `Config.h` (WiFiConfig, ShowConfig, DeviceConfig, or LayoutConfig)
2. Add load/save logic in `Config.cpp`
3. Add API endpoint in `WebServerManager.cpp`
4. Add UI controls in settings page HTML

## Known Constraints

- **No PSRAM**: Adafruit QT Py ESP32-S3 board has no PSRAM - keep memory usage minimal
- **RAM budget**: ~200KB available, current usage ~59KB (18.2%)
- **Flash budget**: 2MB available, current usage ~990KB (47.2%)
- **Stack size**: LED task has 10KB stack, Network task has 10KB stack
- **Queue size**: 5 commands max in ShowController queue (1.25KB total)
- **JSON buffer**: 512 bytes for parameter parsing (StaticJsonDocument<512>)
- **Brightness range**: 0-255, controlled via global brightness setting (not per-show)

## Device Naming

The project was rebranded from "LED Controller" to "ledz". Device IDs follow format `ledz-AABBCC` where AABBCC is the last 3 bytes of MAC address. The mDNS hostname removes the dash: `ledzaabbcc.local`.

See `REBRANDING.md` for complete details on the naming convention.
