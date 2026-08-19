# structured-logging Specification (delta)

## ADDED Requirements

### Requirement: Single logging macro surface

The project SHALL provide a single set of logging macros — `ESP_LOGE`, `ESP_LOGW`, `ESP_LOGI`, `ESP_LOGD`, `ESP_LOGV` — for all log output from project code. No raw `Serial.printf`, `Serial.print`, or `Serial.println` call SHALL appear in `src/**.cpp` outside of the logging dispatch implementation.

#### Scenario: All call sites go through macros
- **WHEN** any file under `src/` (excluding `src/Log.cpp`) is grepped for `Serial\.(printf|print|println)`
- **THEN** no matches are returned
- **THEN** every log call uses `ESP_LOGx(TAG, fmt, ...)` with a file-scope `static const char* TAG`

#### Scenario: Framework `ESP_LOG*` macros are overridden
- **WHEN** `<esp_log.h>` or `<esp32-hal-log.h>` defines `ESP_LOGI/W/E/D/V`
- **THEN** `Log.h` undefs and redefines them so internal framework messages use the same format as project messages

### Requirement: Log line format includes timestamp, level, and tag

Every log line produced on the serial console SHALL include a monotonic millisecond timestamp, a single-letter level identifier, and a short tag identifying the source module.

#### Scenario: Format on Arduino build
- **WHEN** project code calls `ESP_LOGI("net", "Connected to %s", ssid)`
- **THEN** the serial output is `NNNNNNNN I    net Connected to <ssid>\r\n`
- **THEN** `NNNNNNNN` is an 8-digit zero-padded millisecond timestamp from `esp_timer_get_time()`
- **THEN** the level letter is one of `E`, `W`, `I`, `D`, `V`
- **THEN** the tag is right-justified and padded to 6 characters

#### Scenario: Format on native test build
- **WHEN** project code calls `ESP_LOGI(...)` in a native test
- **THEN** no output is produced (the macro is a no-op)

### Requirement: Compile-time gating of debug and verbose levels

`ESP_LOGD` and `ESP_LOGV` SHALL compile to no-ops when `CORE_DEBUG_LEVEL` is below their respective thresholds.

#### Scenario: Default build is silent for D and V
- **WHEN** `CORE_DEBUG_LEVEL` is not defined or is `0`
- **THEN** `ESP_LOGD(...)` and `ESP_LOGV(...)` expand to `do {} while(0)`
- **THEN** the buffer, snprintf call, and `Log::emit` call are absent from the binary for those macros

#### Scenario: Debug level enables D
- **WHEN** `CORE_DEBUG_LEVEL >= 4`
- **THEN** `ESP_LOGD(...)` expands to a full `_LEDZ_LOG` call
- **THEN** the log line appears on the serial console with level letter `D`

#### Scenario: Verbose level enables V
- **WHEN** `CORE_DEBUG_LEVEL >= 5`
- **THEN** `ESP_LOGV(...)` expands to a full `_LEDZ_LOG` call

### Requirement: Single dispatch point for log fan-out

All log lines SHALL pass through a single function `Log::emit(char level, const char* tag, const char* msg)` after macro-level format expansion. Sinks (Serial today; future syslog/file/etc.) live inside this function.

#### Scenario: Adding a sink is a one-function change
- **WHEN** a new sink (e.g., syslog) is added to the project
- **THEN** the addition is contained in `src/Log.cpp` (and any associated header)
- **THEN** no call site outside `src/Log.cpp` requires modification
- **THEN** no macro in `src/Log.h` requires modification

#### Scenario: Sink signature is string-based
- **WHEN** a sink receives a log line
- **THEN** the message is already formatted into a `const char*` buffer
- **THEN** the sink does not need to deal with varargs or format strings

### Requirement: Native test compatibility

The project SHALL compile under the native test environment (`[env:native]` in `platformio.ini`) without any Arduino-specific symbols referenced from logging code.

#### Scenario: No `Serial` symbol in non-ARDUINO builds
- **WHEN** `ARDUINO` is not defined
- **THEN** `Log.h` does not reference `Serial`, `esp_timer_get_time`, or any other Arduino/ESP-IDF symbol
- **THEN** all `ESP_LOG*` macros expand to empty `do {} while(0)` (or `do {} while(0)` with a possibly-empty body)

#### Scenario: Existing native tests still pass
- **WHEN** `pio test -e native` is run
- **THEN** all existing tests pass without modification

### Requirement: One short tag per source file

Each `.cpp` file that logs SHALL declare `static const char* TAG = "<shorttag>";` at file scope and use it as the first argument to every `ESP_LOG*` call in that file. Tags SHALL be a single short word (1-6 lowercase characters), and SHALL be unique across the codebase (except where one module logically spans multiple files).

#### Scenario: Tag taxonomy is enforced by convention
- **WHEN** a new logging call is added
- **THEN** it uses the file-scope `TAG`, not a new ad-hoc string
- **THEN** if the file does not yet have a `TAG`, one is added at file scope

#### Scenario: CaptivePortal shares Network's tag
- **WHEN** code in `src/CaptivePortal.cpp` logs
- **THEN** it uses the same `net` tag as `src/Network.cpp` (one logical module)
