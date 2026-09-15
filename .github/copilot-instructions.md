# Copilot Instructions

## Project Overview

**ledz** is an ESP32-based LED controller with a web interface for WiFi configuration and LED show control. It targets the Adafruit QT Py ESP32-S3 (no PSRAM) and drives WS2812B/NeoPixel LED strips via the [PlatformIO](https://platformio.org/) build system.

**Languages & frameworks**: C++17, Arduino/ESP-IDF, FreeRTOS, ArduinoJson, ESPAsyncWebServer, Adafruit NeoPixel.

## Build & Test Commands

```bash
# Build for ESP32 (primary target)
pio run -e adafruit_qtpy_esp32s3_nopsram

# Build for native (test-only, no hardware required)
pio run -e native

# Run all tests (native only)
pio test -e native

# Run a specific test suite
pio test -e native -f test_color
pio test -e native -f test_jump
pio test -e native -f test_smooth_blend

# Verbose test output
pio test -e native -v

# Upload to device
pio run -e adafruit_qtpy_esp32s3_nopsram -t upload

# Monitor serial output
pio device monitor
```

Tests live in `test/test_*/`. Each subdirectory is an independent test suite. Network/webserver code is **not** compiled for native — only shows, color utilities, and strip abstractions are tested natively. Always run `pio test -e native` before declaring a change complete.

## Repository Layout

```
src/                    Core components (main.cpp, Config, Network, ShowController, ShowFactory, WebServerManager)
src/show/               LED show implementations (each show is a separate .h/.cpp pair)
src/strip/              Strip abstraction (Strip.h, Base.h/cpp, Layout.h/cpp)
src/support/            Utilities (SmoothBlend for color transitions)
src/OTAUpdater.*        OTA firmware update logic
src/OTAConfig.h         OTA configuration (GitHub repo, firmware version)
include/                Shared headers
test/test_*/            Unit tests (PlatformIO native environment)
docs/                   Documentation (SHOW_PARAMETERS.md, MDNS.md, OTA docs)
platformio.ini          Build environments (adafruit_qtpy_esp32s3_nopsram, native)
partitions.csv          Custom flash partition table (4MB flash)
```

## Architecture

### Dual-Core Threading Model

- **Core 0 (Network Task)**: `Network::task()` — WiFi, NTP, mDNS, web server.
- **Core 1 (LED Task)**: `ledShowTask()` — renders LED animations at ~100 Hz (10 ms cycle).

**Thread-safety rule**: `WebServerManager` (Core 0) must **never** directly modify shows or LED state. It queues commands via `ShowController::queueShowChange()`. The LED task processes them via `ShowController::processCommands()` each iteration.

### Memory Management (strict rules)

- **Zero raw owning pointers** — all dynamic memory is managed via `std::unique_ptr`.
- Transfer ownership with `std::move()`; never copy `unique_ptr`.
- Non-owning access uses references (`T&`), never raw pointers.
- `NEVER use new` without immediately wrapping in `std::unique_ptr`.

### Show System

All shows inherit from `Show::Show` with virtual `execute(Strip::Strip&, Iteration)`. Shows are created by `ShowFactory::createShow()` (returns `std::unique_ptr<Show::Show>&&`) and owned by `ShowController`.

**Adding a new show**:
1. Create `src/show/MyShow.h` and `src/show/MyShow.cpp` inheriting from `Show::Show`.
2. Register in `ShowFactory.cpp` constructor with a lambda factory.
3. Optionally add JSON parameter parsing in `ShowFactory::createShow()` using `doc["key"] | default` syntax.
4. Optionally add UI controls in `WebServerManager.cpp`.
5. Guard all ESP32-specific code with `#ifdef ARDUINO`.

### Configuration & Persistence

`Config::ConfigManager` (singleton) persists settings to ESP32 NVS (namespace `"ledctrl"`):
- **WiFiConfig**: SSID, password, configured flag, failure counter.
- **DeviceConfig**: brightness (0–255), device_name, device_id, num_pixels.
- **ShowConfig**: current_show, params_json, auto_cycle settings.
- **LayoutConfig**: reverse, mirror, dead_leds.

Always call `config.begin()` before use.

### Web API Key Endpoints

| Endpoint | Method | Purpose |
|---|---|---|
| `/` | GET | Main control UI |
| `/config` | GET | WiFi config page (AP mode) |
| `/api/show` | POST | Change show (JSON: `{name, params}`) |
| `/api/brightness` | POST | Set brightness |
| `/api/status` | GET | Device status |
| `/api/settings/factory-reset` | POST | Factory reset |
| `/api/ota/check` | GET | Check GitHub for firmware updates |
| `/api/ota/update` | POST | Download and flash update |

The entire web interface is embedded as C++ raw string literals in `WebServerManager.cpp`.

## Key Constraints

- **No PSRAM** — keep memory usage minimal (RAM budget ~200 KB, current usage ~59 KB).
- **JSON buffer**: `StaticJsonDocument<512>` for parameter parsing.
- **Stack sizes**: LED task and Network task each have 10 KB stacks.
- **OTA partition size**: firmware must stay under 1,856 KB.
- Use `Serial.printf()` for debug logging (not `std::cout`).
- Platform-specific code must be guarded with `#ifdef ARDUINO`.
- Shows with parameters must **not** auto-start when selected from the dropdown — they wait for the user to click "Apply Parameters".
