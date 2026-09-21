[![Lines of Code](https://sonarcloud.io/api/project_badges/measure?project=oetztal_ledz&metric=ncloc)](https://sonarcloud.io/summary/new_code?id=oetztal_ledz)
[![Coverage](https://sonarcloud.io/api/project_badges/measure?project=oetztal_ledz&metric=coverage)](https://sonarcloud.io/summary/new_code?id=oetztal_ledz)
[![Duplicated Lines (%)](https://sonarcloud.io/api/project_badges/measure?project=oetztal_ledz&metric=duplicated_lines_density)](https://sonarcloud.io/summary/new_code?id=oetztal_ledz)
[![Maintainability Rating](https://sonarcloud.io/api/project_badges/measure?project=oetztal_ledz&metric=sqale_rating)](https://sonarcloud.io/summary/new_code?id=oetztal_ledz)
[![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=oetztal_ledz&metric=alert_status)](https://sonarcloud.io/summary/new_code?id=oetztal_ledz)

# ledz

[ESP32-based LED controller](https://oetztal.github.io/ledz/)
with web interface for WS2812B/NeoPixel LED strips.

## Features

- **15 LED shows** - Rainbow, Fire, Wave, Starlight, Mandelbrot, and more
- **Web interface** - Control from any device on your network
- **Presets** - Save and recall up to 8 complete configurations
- **Touch control** - Capacitive touch pins to load presets without WiFi
- **Timers & Schedules** - Countdown timers, plus schedules that switch shows at a set time on chosen weekdays and can be paused
- **OTA updates** - Update firmware over WiFi from GitHub releases
- **Easy setup** - Captive portal for WiFi configuration

## Hardware

**Supported board**: Adafruit QT Py ESP32-S3 (no PSRAM)

| Specification | Value |
|---------------|-------|
| LED type | WS2812B / NeoPixel |
| Max LEDs | 300 (configurable) |
| LED pin | GPIO 39 (onboard) or GPIO 35 (external) |

## Wiring

The ESP32-S3 GPIO outputs 3.3 V. The WS2812B spec requires a logic-high of
≥ 0.7 × VDD (3.5 V at 5 V supply), so a direct 3.3 V → 5 V connection is
technically out-of-spec and is the most common cause of *wrong pixels lighting
up in the same area* on long strips. Add a level shifter and a series resistor
on the data line:

```
                  +5V
                   │
            ┌──────┴──────┐
            │             │
  ESP32-S3  │   330 Ω    ┌┴─────────────┐
  GPIO ─────┴───/\/\/────┤1A    74HCT125├── 1Y ────── DIN  LED Strip
                          │             │
                          │  OE ──┐     │
                          │       GND   │
                          └─────────────┘
            GND ─────────────────────────────────── GND
            +5V ────────────────┬────────────────── +5V
                               │
                          ┌────┴────┐
                          │ 1000 µF │  (across +5V / GND at strip input)
                          └─────────┘
```

**Components**

- **74HCT125** (or 74HCT245) — non-inverting 3.3 V → 5 V level shifter. Tie the
  output-enable pin (`OE`) to GND so the buffer is always active.
- **330 Ω resistor** in series with the data line, placed as close to the
  ESP32 GPIO as possible. Dampens reflections on the data wire that can
  re-trigger pixels and corrupt the bitstream.
- **1000 µF capacitor** across +5 V and GND at the strip's power input.
  Buffers the WiFi TX current spikes that would otherwise dip the rail and
  cause the LEDs to latch wrong bits.

**Pin assignments** (default, configurable in Settings → LED Pin):

- GPIO 39 — onboard NeoPixel on the QT Py ESP32-S3 (no external wiring needed)
- GPIO 35 — external strip, route through the level shifter as shown above

For long strips (≥ 60 LEDs) or strips where the wrong-pixel area is in the
back half, also inject +5 V / GND at the far end of the strip. Voltage sags
along the length, and the back pixels latch wrong bits under load.

## Getting Started

### Build & Upload

```bash
pio run -e adafruit_qtpy_esp32s3_nopsram -t upload
```

### Initial Setup

1. Power on the device
2. Connect to the WiFi network `ledz-XXXXXX` (where XXXXXX is the device ID)
3. A captive portal opens automatically, or navigate to `192.168.4.1`
4. Enter your WiFi credentials
5. The device restarts and connects to your network
6. Access the web interface at `ledzxxxxxx.local` or check your router for the IP

## Web Interface

| Page | Description |
|------|-------------|
| Control | Select shows, adjust parameters, manage presets |
| Timers | Set countdown timers and schedules |
| Settings | Configure WiFi, brightness, LED count, OTA updates |
| About | Device information and diagnostics |

## LED Shows

| Show | Description |
|------|-------------|
| Solid | Static color or multi-color sections (flags) |
| Rainbow | Cycling rainbow spectrum |
| Fire | Realistic fire simulation |
| Wave | Propagating sine-wave patterns |
| Starlight | Twinkling star effect |
| ColorRanges | Multi-color gradient sections |
| TwoColorBlend | Smooth gradient between two colors |
| ColorRun | Running color animation |
| Jump | Bouncing light effects |
| TheaterChase | Marquee-style chase |
| Stroboscope | Flashing strobe light |
| MorseCode | Text as blinking morse code |
| Chaos | Chaotic logistic map patterns |
| Mandelbrot | Fractal zoom visualization |

## Timers

- **Countdown timers** - Turn off or load a preset after a duration
- **Schedules** - Trigger at a specific time on selected weekdays; edit in place or pause without deleting
- Up to 12 concurrent timers and schedules
- Actions: Turn off LEDs or load a saved preset

## API

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/show` | POST | Change show with JSON parameters |
| `/api/brightness` | POST | Set brightness (0-255) |
| `/api/status` | GET | Current show and device status |
| `/api/presets` | GET | List saved presets |
| `/api/presets` | POST | Save a preset |
| `/api/timers` | GET | List active timers |
| `/api/timers/countdown` | POST | Set countdown timer |
| `/api/timers/schedule` | POST | Set or update a schedule (optional `days` weekday mask); `/api/timers/alarm` is kept as an alias |
| `/api/timers/pause` | POST | Pause or resume a schedule |
| `/api/touch` | GET | Touch config and current values |
| `/api/touch` | POST | Update touch settings |
| `/api/ota/check` | GET | Check for firmware updates |
| `/api/ota/update` | POST | Install firmware update |

## Project Structure

```
src/
  main.cpp              # Entry point
  Config.h/cpp          # NVS persistence
  Network.h/cpp         # WiFi, mDNS, NTP
  WebServerManager.cpp  # Web interface & API
  ShowController.cpp    # Show management
  ShowFactory.cpp       # Show creation
  TimerScheduler.cpp    # Timer system
  TouchController.cpp   # Capacitive touch input
  OTAUpdater.cpp        # Firmware updates
  show/                 # LED show implementations
  strip/                # LED hardware abstraction
data/
  control.html          # Main control page
  settings.html         # Settings page
  timers.html           # Timer scheduler
docs/
  SHOW_PARAMETERS.md    # Show configuration guide
  OTA_FIRMWARE_UPDATES.md
```

## Architecture

- **Dual-core**: Network tasks on Core 0, LED rendering on Core 1
- **100Hz refresh**: Smooth animations at 10ms cycle time
- **Thread-safe**: FreeRTOS queues for inter-core communication
- **Persistent config**: All settings stored in ESP32 NVS

## Development

### Run Tests

```bash
pio test -e native
```

### Test Coverage

To track test coverage, you need `lcov` installed (on macOS: `brew install lcov`).

Generating the report is a separate step from running the tests. PlatformIO
builds and runs each test directory in turn, so there is no build hook that
fires after the *last* test binary has exited — and that is the only point at
which the coverage data is complete.

1. Run tests:
   ```bash
   pio test -e native
   ```
2. Build the report:
   ```bash
   pio run -e native -t coverage      # or: python3 scripts/coverage_report.py
   ```
3. Open it:
   ```bash
   open coverage_report/index.html
   ```

The report covers `src/` only, excluding the generated web-asset headers. Stale
coverage data from recompiled objects is discarded automatically after each
link; without that, editing a test makes the coverage runtime fail its merge
and, on macOS, segfault at exit *after* every test has already passed.

The script also writes `coverage-generic.xml` for SonarCloud. SonarCloud's
CFamily sensor silently ignores `sonar.cfamily.coverage.reportPaths` from
6.79.0 onwards, and `sonar.gcov.reportsPath` is deprecated; the Generic
Coverage XML sensor reads the file via `sonar.coverageReportPaths`.

### Web Assets

Web files in `data/` are automatically minified and gzip-compressed into C++ header files during the build process. No manual steps required.

## License

[Apache License 2.0](LICENSE)

