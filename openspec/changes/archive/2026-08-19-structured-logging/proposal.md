## Why

The ledz codebase has ~196 `Serial.printf/print/println` call sites across 15 files, with no log levels, no timestamps, no tags, and no central dispatch. The result is a serial console that is:

1. **Unfilterable.** Every call fires unconditionally. The NTP update log (every 5 minutes) and the LED task's per-iteration log cannot be silenced without recompiling.
2. **Unstructured.** Each module reinvents its own prefix convention (`"Network: "`, `"Config: "`, `"ColorRanges: "`, ...). Grep is the only way to filter, and the prefix often sits inside the format string instead of being structural.
3. **Unreadable across time.** No timestamps means the only way to correlate two events is to count lines, and only works on a single contiguous dump.
4. **Hard to extend.** The OTA module already needed structured logging badly enough that the team invented a project-local `OTA_LOG` macro (`src/OTAConfig.h:99`). That macro is the shape of the right abstraction, just scoped to one module.
5. **Inconsistent across cores.** Core 0 (Network) and Core 1 (LED) both log directly to `Serial`, with no synchronization — output from the two cores can interleave byte-by-byte inside a single `printf` line.

The reference implementation in `https://github.com/wuan/klimacontrol/blob/main/src/Log.h` solves all five problems with one header file. This change adopts that pattern, adapts it for ledz's no-PSRAM S3, and uses the migration as an opportunity to apply level discipline to the existing log volume.

## What Changes

- **New module `src/Log.{h,cpp}`** that exposes `ESP_LOGE/W/I/D/V(tag, fmt, ...)` macros matching the ESP-IDF convention. Undefs the framework's default `ESP_LOG*` macros before redefining them, so internal framework messages (Preferences, WiFi) pick up the same format.
- **Single dispatch point (`Log::emit`)** receives a pre-formatted message + level + tag. Today: prints to `Serial` with `%08lu %c %6s %s\r\n` (millisecond timestamp + level letter + padded tag + body). Tomorrow: a syslog/file sink can be added inside this one function without touching any caller.
- **`CORE_DEBUG_LEVEL` compile gate** matching klimacontrol: `D` requires `>= 4`, `V` requires `>= 5`, default `0`. Production builds compile to zero cost for `D/V` calls.
- **Native test stubs** — all five `ESP_LOG*` macros expand to empty `do {} while(0)` when `ARDUINO` is not defined. Native tests no longer need a `Serial` stub.
- **Tag taxonomy** — one short tag per file: `main`, `net`, `http`, `cfg`, `ctrl`, `show`, `timer`, `touch`, `led`, `strip`, `ota`.
- **Migration of all ~196 `Serial.printf/print/println` call sites** to `ESP_LOG*`. Each site gets a level chosen per the rule "E = broken, W = recovered/wrong, I = lifecycle, D = periodic detail". Multi-line `Serial.print` chains get combined or split into multiple `ESP_LOG*` calls as appropriate.
- **`OTA_LOG` macro folded into `ESP_LOG*` with `tag = "ota"`**. The 87 OTA call sites pick up level discipline (most become `W`, the `OTA diag:` block becomes `D`). The `OTA_DEBUG_LOGGING` flag is removed; gating moves to `CORE_DEBUG_LEVEL`.

## Capabilities

### New Capabilities

- `structured-logging`: Defines the project logging conventions — `ESP_LOG*` macro surface, level semantics, tag taxonomy, compile-time debug gating, native-test compatibility, and the single-dispatch architecture that prepares the codebase for future sink fan-out (syslog, file, etc.).

### Modified Capabilities

None.

## Impact

- `src/Log.h` (new) — macro definitions and native stubs.
- `src/Log.cpp` (new) — `Log::emit` implementation.
- `src/OTAConfig.h` — remove `OTA_LOG` macro and `OTA_DEBUG_LOGGING` flag.
- `src/OTAUpdater.cpp` — replace 87 `OTA_LOG(...)` calls with `ESP_LOG*(...)`; add `static const char* TAG = "ota";`.
- `src/main.cpp` — replace 8 calls; add `#include "Log.h"`.
- `src/Network.cpp` — replace 71 calls; add TAG; classify NTP tick and WiFi diag block as `D`.
- `src/CaptivePortal.cpp` — replace 3 calls.
- `src/WebServerManager.cpp` — replace 16 calls.
- `src/Config.cpp` — replace 8 calls; add TAG.
- `src/ShowController.cpp` — replace 35 calls.
- `src/ShowFactory.cpp` — replace 15 calls.
- `src/TimerScheduler.cpp` — replace 10 calls.
- `src/TouchController.cpp` — replace 10 calls.
- `src/task/LedShow.cpp` — replace 4 calls; classify per-iteration line as `D`.
- `src/strip/Base.cpp` — replace 1 call.
- `src/show/ColorRanges.cpp` — replace 10 calls; classify init dump as `D`.
- `src/show/Mandelbrot.cpp`, `src/show/Jump.cpp` — convert the commented/diagnostic calls (low priority).
- `platformio.ini` — no change required (Log.h is included only from sources already compiled into both envs).
- `test/` — no change; native tests already exclude the files that were using `Serial.printf` and now use `ESP_LOGI` stubs.

**Backward compatibility:** The serial wire format changes (timestamps + level letters + tags are added; existing module-prefix strings inside format strings are removed). Users tailing the serial monitor today will need to update their parsing if they have any. No persistent state changes. No API endpoint changes. No partition table change.

**Flash/RAM budget:** Net flash impact is small. Each `Serial.printf(fmt, args...)` site becomes `_LEDZ_LOG(letter, tag, fmt, args...)` — an extra `snprintf` into a 256-byte stack buffer plus a function call. On the LED task's 10 KB stack, the 256-byte buffer is 2.5% per call (only allocated when the call actually fires; gated to zero for `D/V` at production). The `ESP_LOGD/V` macros compile to `do {} while(0)` at production, so files with debug-heavy logging see smaller binary size despite more lines of code.
