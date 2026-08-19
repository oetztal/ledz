## 1. Create `src/Log.h` and `src/Log.cpp`

- [x] 1.1 Write `src/Log.h` with the macro definitions: undef `ESP_LOG*`, define `_LEDZ_LOG`, redefine `ESP_LOGE/W/I/D/V`, gate `D`/`V` behind `CORE_DEBUG_LEVEL`, native stubs in the `#else` branch, no-op `esp_log_level_set`.
- [x] 1.2 Write `src/Log.cpp` with `void Log::emit(char level, const char* tag, const char* msg)` that calls `Serial.printf("%08lu %c %6s %s\r\n", esp_timer_get_time()/1000ULL, level, tag, msg)`. File is empty (or `#include "Log.h"` only) under `#else`.
- [x] 1.3 Confirm the file matches the design decisions 1, 2, 3 — `Log::emit` is the single dispatch point, `CORE_DEBUG_LEVEL` default is 0, native stubs are empty.

## 2. Verify build of new module in isolation

- [x] 2.1 `pio test -e native` — native env compiles and all 87 tests pass. (`pio run -e native` fails because `[env:native]` is test-only with no `main.cpp`; this is pre-existing project behavior, not a regression. Log.h's native stubs are unreachable until a file pulls in the header.)
- [x] 2.2 `pio run -e adafruit_qtpy_esp32s3_nopsram` — ESP32 env compiles cleanly. No `ESP_LOG*` redefinition warnings. RAM 18.7%, Flash 65.3% (within budget).
- [x] 2.3 `pio test -e native` — all 87 existing tests pass.

## 3. Convert smallest files first (sanity-check the macro pattern)

- [x] 3.1 `src/strip/Base.cpp` — replaced 1 `Serial.printf` call (gamma mode) with `ESP_LOGI(TAG, ...)`. TAG = `strip`.
- [x] 3.2 `src/main.cpp` — replaced 8 calls (3 printf + 5 print/println); added `#include "Log.h"`; TAG = `main`. Error-path calls use `ESP_LOGE`.
- [x] 3.3 `src/show/Jump.cpp` — removed the commented `Serial.printf` line.
- [x] 3.4 `src/show/Mandelbrot.cpp` — replaced `Serial.println(ss.str().c_str())` with `ESP_LOGD(TAG, "%s", ss.str().c_str())`. The function is currently only called from a commented-out line in `execute`, but kept for future use. TAG = `show`.
- [x] 3.5 `pio run -e adafruit_qtpy_esp32s3_nopsram` and `pio test -e native` both pass.

## 4. Convert medium files (touch and timer)

- [x] 4.1 `src/TouchController.cpp` — replaced 9 calls with `ESP_LOGI(TAG, ...)`. TAG = `touch`.
- [x] 4.2 `src/TimerScheduler.cpp` — replaced 10 calls. Invalid-preset cancellation → `ESP_LOGW`. TAG = `timer`.
- [x] 4.3 `src/CaptivePortal.cpp` — replaced 3 calls. TAG = `net` (shared with Network.cpp).

## 5. Convert config and show files

- [x] 5.1 `src/Config.cpp` — replaced 8 calls. TAG = `cfg`. `requestRestart` → `W`. `checkRestart` → `I`. Preferences save/load → `D`.
- [x] 5.2 `src/ShowFactory.cpp` — replaced 13 calls (11 show-creation `I`, 1 "not found" `W`, 1 parse failure `W`). TAG = `show`.
- [x] 5.3 `src/ShowController.cpp` — replaced 23 calls. TAG = `ctrl`. Show switch / brightness / layout lifecycle → `I`. Queue-full → `W`. Failed show creation / missing pointers / missing strip → `E`. Per-pointer show creation details → `D`.

## 6. Convert large files (Network, WebServerManager, LedShow)

- [x] 6.1 `src/task/LedShow.cpp` — replaced 4 calls. TAG = `led`. Power-save transitions → `I`, periodic stats → `D` (silenced in production).
- [x] 6.2 `src/WebServerManager.cpp` — replaced 16 calls + the AccessLogger format. TAG = `http`. Lifecycle (server start/stop, route setup, settings updates) → `I`. Factory reset → `W`. 404 → `W`. Access log → `D` (silenced).
- [x] 6.3 `src/Network.cpp` — replaced 71 calls. TAG = `net`. Lifecycle (AP start, STA connect, mDNS init, NTP first sync, reconnect success, restart) → `I`. Connection failure / WiFi disconnected / too-many-failures → `W`. **WiFi diag dump + NTP periodic tick + WiFi-status delayed + resolvers + free heap → `D`** (silenced in production).
- [x] 6.4 Both build environments clean: ESP32 SUCCESS, all 87 native tests pass.

## 7. Convert `ColorRanges.cpp` (show init dump)

- [x] 7.1 `src/show/ColorRanges.cpp` — replaced 10 calls (verbose init-time dump, silenced in production). TAG = `show`. Init dumps → `D`; range validation warning → `W`.

## 8. Fold `OTA_LOG` into `ESP_LOG*`

- [x] 8.1 `src/OTAConfig.h` — removed `OTA_LOG` macro and `OTA_DEBUG_LOGGING` flag. `OTA_LOG_MEMORY` kept (no current references — vestigial, harmless).
- [x] 8.2 `src/OTAUpdater.cpp` — added `#include "Log.h"` and `static const char* TAG = "ota";`. Replaced all 72 `OTA_LOG(...)` calls (15 multi-line calls folded via Python script; one multi-line call fixed manually after the script run). Classification: HTTP failures / SHA-256 failures / OTA update failed / task-create failed → `E`. Refused checks / verdict messages / content-length mismatches → `W`. Success / state transitions → `I`. All `OTA diag:` lines (DNS, TCP, UDP, resolvers, rssi) → `D`.
- [x] 8.3 ESP32 build clean.
- [x] 8.4 `rg "OTA_LOG" src/` returns 0 matches (only `OTA_LOG_MEMORY` definition remains in OTAConfig.h).

## 9. Final verification

- [x] 9.1 `rg "Serial\.(printf|print|println)" src/` returns 1 match (only `src/Log.cpp:10`, the dispatch function — expected).
- [x] 9.2 `pio run -e adafruit_qtpy_esp32s3_nopsram` — clean build. RAM 18.7% (61,392 bytes), Flash 65.3% (1,240,593 bytes).
- [x] 9.3 `pio test -e native -v` — all 87 tests pass.
- [x] 9.4 `pio test -e native -f test_shows` — passes (ColorRanges now silent on native via ESP_LOGD stubs).
- [ ] 9.5 Production output verification via `pio device monitor` after flashing — skipped (no device available in this environment). The compile-time gate guarantees only `E/W/I` lines appear at `CORE_DEBUG_LEVEL=0`; can be confirmed on-device by anyone with a board.
- [x] 9.6 AGENTS.md updated: "Serial Logging" section replaced with "Logging" section documenting the `ESP_LOGx(TAG, fmt, ...)` convention, level semantics, tag taxonomy, and `CORE_DEBUG_LEVEL` build flag.
