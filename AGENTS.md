# AGENTS.md

Guidance for AI coding agents working in this repository.

## Project Overview

**ledz** is an ESP32-based LED controller with a web interface for WiFi configuration and show control. It runs on the Adafruit QT Py ESP32-S3 (no PSRAM) and drives WS2812B/NeoPixel strips.

Key traits: dual-core (Core 0 network/web, Core 1 LED rendering), FreeRTOS-queue command passing, embedded web UI, NVS persistence, mDNS discovery, AP-mode captive portal setup.

## Build, Test, Flash

```bash
pio run -e adafruit_qtpy_esp32s3_nopsram              # ESP32 firmware
pio run -e adafruit_qtpy_esp32s3_nopsram -t upload     # flash device
pio device monitor                                     # serial output
pio run -e native                                      # native build (tests only)
pio test -e native                                     # run all tests
pio test -e native -f test_color                       # one suite
```

Debug logs are compiled out by default (`CORE_DEBUG_LEVEL=0`); enable with `pio run -e adafruit_qtpy_esp32s3_nopsram -DCORE_DEBUG_LEVEL=4`.

## Architecture

### Threading (critical)

- **Core 0** runs `Network::task()` (WiFi, NTP, mDNS, webserver).
- **Core 1** runs `ledShowTask()` (~100Hz LED rendering) and owns `currentShow`.
- `WebServerManager` must **never** touch shows or LED state directly — it calls `ShowController::queueShowChange()`, which enqueues a `ShowCommand`.
- The LED task processes commands each iteration via `ShowController::processCommands()`.
- `showTaskHandle` can be suspended (e.g. factory reset) before manipulating LED state.

### Ownership

Zero raw pointers for owned resources. Use `std::unique_ptr` + move semantics; references (`&`) for non-owning access. Never `new` without immediately wrapping it. Factories return `std::unique_ptr<T>&&`.

```
main.cpp
├─ ShowController (owns currentShow, baseStrip, layout)
│   ├─ ShowFactory&   (creates shows)
│   └─ Config&        (NVS persistence)
└─ Network (owns webServer)
    ├─ Status&, Config&
    └─ WebServerManager (moved in; refs Config, Network, ShowController)
```

**Strip**: `baseStrip` wraps `Adafruit_NeoPixel`; `layout` decorates it (reverse, mirror, dead-LED masking). Layout is recreated at runtime via `layout.reset(new Strip::Layout(*baseStrip, ...))`.

### Shows

All shows inherit `Show::Show` with virtual `execute(Strip::Strip&, Iteration)`.

Creation flow: web POST `/api/show` → `queueShowChange()` → LED task `processCommands()` → `ShowFactory::createShow()` (parses JSON, returns `unique_ptr` rvalue ref) → `currentShow = std::move(newShow)` → params saved to NVS.

**Adding a show**: create `src/show/MyShow.{h,cpp}`, register with a lambda in `ShowFactory.cpp`, add JSON parsing and UI controls (optional), include in `main.cpp`. If it has params, add it to `updateParameterVisibility()` and the `showsWithParams` array so it doesn't auto-start on dropdown selection.

Parse JSON defaults with the `|` operator: `uint8_t r = doc["r"] | 255;`.

### Config (NVS namespace `ledctrl`)

`WiFiConfig` (ssid, password, configured, failure counter), `ShowConfig` (current_show, params_json, auto_cycle), `DeviceConfig` (brightness, device_name, device_id, num_pixels), `LayoutConfig` (reverse, mirror, dead_leds). Access via `Config::ConfigManager` singleton; call `config.begin()` in setup.

### Network

- **AP mode**: first boot or 3 failed connections. SSID `ledz AABBCC`, IP 192.168.4.1, captive portal. Restarts after WiFi config received.
- **STA mode**: joins configured network, advertises `ledz-aabbcc.local` (mDNS), refreshes NTP every 300s, auto-reconnects and increments the failure counter.

### Web

The entire UI is embedded as C++ raw string literals in `WebServerManager.cpp` (no filesystem). Key endpoints: `GET /`, `GET /config`, `POST /api/show`, `POST /api/brightness`, `GET /api/status`, `POST /api/settings/factory-reset`, and `/api/ota/*` (see `docs/OTA_*.md`).

To add show params to the UI: add the HTML params section, a case in `updateParameterVisibility()`, and an `applyShowNameParams()` JS function that POSTs to `/api/show`.

## Conventions

### Logging

Use `ESP_LOGx(TAG, fmt, ...)` from `src/Log.h`; never `Serial.printf` outside `src/Log.cpp`. Each logging `.cpp` declares `static const char* const TAG = "<shorttag>";` (1–6 lowercase chars: `main`, `net`, `http`, `cfg`, `ctrl`, `show`, `timer`, `touch`, `led`, `strip`, `ota`). Levels: `E` broken, `W` recovering, `I` lifecycle/state changes, `D` per-iteration detail, `V` unused. Log show creation, network/AP transitions, config changes, OTA transitions, factory reset.

### Platform guards

Wrap ESP32-only code in `#ifdef ARDUINO`. Native builds cover shows, color utilities, and strip abstractions — not network/webserver.

### Factory reset

Suspend the LED task before clearing the strip to avoid a race:

```cpp
vTaskSuspend(showTaskHandle);
showController->clearStrip();
delay(500);
config.reset();
ESP.restart();
```

### Formatting / pre-commit

A `.clang-format` config matches the existing style (4-space indent, K&R-ish braces, pointer/reference left). Run `pre-commit install` once after cloning; `git commit` then fails if staged C/C++ under `src/`, `test/`, or `include/` is not clang-format clean, if `scripts/*.sh` is not shellcheck clean, or if `openspec/**` doesn't pass `openspec validate --all --strict`. Format manually with `clang-format -i <file>`; bypass with `git commit --no-verify` only when you really mean it.

**Pinned to clang-format 19.1.7** — newer majors (23+) reformat macro line-continuations differently, so locally install via `pip install clang-format==19.1.7` (or `brew install llvm@19`); the CI workflow installs the same wheel. OpenSpec validation is pinned via `OPENSPEC_VERSION` in `scripts/validate-openspec.sh`.

## Static Analysis (SonarCloud)

SonarCloud scans every push and same-repo PR; the project key is `oetztal_ledz`. When asked to check/review/triage Sonar issues, query the public Web API (no token needed) rather than the JavaScript web UI. Results reflect the last analyzed commit, which can lag `HEAD`.

```bash
# Quality gate
curl -s "https://sonarcloud.io/api/qualitygates/project_status?projectKey=oetztal_ledz" | jq '.projectStatus'
# Open issues (ps max 500); add severities=, types=, rules=, inNewCodePeriod=true to filter
curl -s "https://sonarcloud.io/api/issues/search?componentKeys=oetztal_ledz&resolved=false&ps=500"
```

See README's "Static Analysis (SonarCloud)" section for the full query parameters and metrics endpoint.

## File Locations

- Core: `src/` (main.cpp, Config, Network, ShowController, ShowFactory, WebServerManager)
- Shows: `src/show/` (one .h/.cpp pair each)
- Strip: `src/strip/` (Strip.h, Base, Layout)
- Utilities: `src/support/`
- Tests: `test/test_*/` (independent suites)
- Docs: `docs/`

## Common Modifications

- **Add a ColorRanges flag preset**: add a button in the HTML plus a `loadMyFlag()` JS function that sets the color inputs and `colorRangesRanges`.
- **Change dropdown show order**: reorder registration in `ShowFactory.cpp` (default show is set separately in `ShowController.cpp`).
- **Add a device setting**: add the struct field in `Config.h`, load/save in `Config.cpp`, an endpoint in `WebServerManager.cpp`, and UI controls.

## OTA Updates

GitHub-release OTA via `src/OTAUpdater.{h,cpp}` and `src/OTAConfig.h`. Flow: check → download `.bin` over HTTPS → stream to the inactive partition (app0↔app1) → restart → optional `confirmBoot()` disables rollback. Endpoints under `/api/ota/`. Docs: `docs/OTA_FIRMWARE_UPDATES.md`, `docs/OTA_QUICK_START.md`.

## Constraints

- No PSRAM: keep memory minimal (~200KB RAM budget; ~59KB used).
- Flash ~2MB budget (~990KB used).
- LED and Network tasks: 10KB stack each; show queue holds 5 commands.
- JSON parsing uses ArduinoJson 7 `JsonDocument` (heap-allocated, grows on demand; no fixed buffer).
- Brightness is global (0–255), not per-show.

## Device Naming

Device ID is the last 3 MAC bytes (`AABBCC`, `DeviceId::getDeviceId()`). The `ledz` prefix belongs to consumers: mDNS hostname `ledz-aabbcc.local` (dash), AP SSID / mDNS instance `ledz AABBCC` (space). See `REBRANDING.md`.
