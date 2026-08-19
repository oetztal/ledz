## Context

ledz today logs with raw `Serial.printf` across 15 source files. The OTA subsystem already invented a project-local `OTA_LOG` macro that is, in shape, exactly what the rest of the codebase needs. This change generalizes that one-module abstraction into a project-wide standard.

The reference implementation is klimacontrol's `src/Log.h` (https://github.com/wuan/klimacontrol/blob/main/src/Log.h). klimacontrol's `Log.h` ships with a syslog UDP sink (`SyslogOutput.h`). This change does not ship the sink — but the architecture is designed so that adding one is a one-function change. No public API is added now that would need to be re-designed later.

The codebase's resource budget is tight (no PSRAM, ~200 KB RAM, ~990 KB flash used of 2 MB). Every design choice below was weighed against this budget.

## Goals / Non-Goals

**Goals:**
- All project logging goes through `ESP_LOG*` macros — no raw `Serial.printf` for log output.
- Log lines carry a timestamp, level letter, and tag — readable and greppable.
- Compile-time gating of debug/verbose levels via `CORE_DEBUG_LEVEL`.
- Native test builds compile without any Arduino dependency for logging.
- Migration of all ~196 existing call sites, with level discipline applied to the noisy ones (NTP tick, WiFi diagnostics, LED per-iteration log).
- Single dispatch point (`Log::emit`) so future sinks (syslog, file, MQTT, ...) can be added without touching call sites or the macro layer.
- Sink-ready without shipping one: the dispatch function's signature is fixed and sink-friendly.

**Non-Goals:**
- Shipping a syslog/file/MQTT sink today.
- Runtime log-level filtering per tag (compile-time only).
- Structured (JSON) log output — the format remains a human-readable line.
- Persisting logs to flash.
- Changing the existing 4 MB partition table or any flash layout.
- Per-show or per-module dynamic log control (e.g., "disable color logs at runtime").
- Modifying any web API endpoints, configuration persistence schema, or NVS layout.

## Decisions

### 1. Follow klimacontrol's `Log.h` shape, not invent a new one

**Decision:** Adopt the `ESP_LOGx(tag, fmt, ...)` shape with `x ∈ {E, W, I, D, V}`. Same undef-then-redefine pattern. Same `CORE_DEBUG_LEVEL` gating for `D` and `V`. Same native-stub behavior.

**Why:** The reference is already used by one project, already documented, and already proven to solve exactly this problem. Inventing a divergent API (`LEDZ_LOG_INFO`, custom level letters, different formatting) gains nothing and creates a maintenance tax.

**Alternatives considered:**
- *Custom level names (e.g., `LEDZ_LOG_ERR/WARN/INFO/DBG`).* Same shape, different names. Pure churn — no value.
- *No undef of framework `ESP_LOG*`.* Lets framework messages use ESP-IDF's default format (with colors, etc.) while project messages use the new format. Inconsistent and makes serial output harder to scan.

### 2. Function-call dispatch (`Log::emit`), not inline sink-conditional

**Decision:** The `_LEDZ_LOG` macro pre-formats the message into a 256-byte stack buffer via `snprintf`, then calls `Log::emit(letter, tag, msg)`. All sink logic (Serial output today; future syslog/file) lives inside `Log::emit`.

```cpp
#define _LEDZ_LOG(letter, tag, format, ...) do {                \
    char _log_buf[256];                                        \
    snprintf(_log_buf, sizeof(_log_buf), format, ##__VA_ARGS__);\
    Log::emit(#letter[0], tag, _log_buf);                      \
} while(0)
```

**Why:** This is the minimum architecture that satisfies "sink-ready without shipping one". When a syslog sink lands, it adds one line inside `Log::emit`. No macro surgery. No public API. The pre-formatted buffer means sinks receive a `const char*` — they never deal with varargs, `va_list`, or format strings, so adding a second sink is free in the macro layer.

**Alternatives considered:**
- *Forward varargs via `va_list` to `Log::emit`.* Saves the buffer but `va_list` can only be traversed once — multi-sink fan-out requires re-`va_start` between sinks, which is fragile. The 256-byte buffer is cheap.
- *Sink-registry API (`Log::addSink(sink)`).* Overdesigned for a codebase with one sink today and one likely sink tomorrow.
- *Sink check inline in the macro (`if (SyslogOutput::isActive()) ...`).* Couples the macro to a specific sink's existence. The whole point of `Log::emit` is to keep that out of the macro.

### 3. `CORE_DEBUG_LEVEL = 0` default; no runtime level filter

**Decision:** Default `CORE_DEBUG_LEVEL = 0`. `D` and `V` macros compile to `do {} while(0)` below thresholds 4 and 5 respectively. There is no runtime API to change a module's log level.

**Why:** Runtime level filtering on ESP32-S3 with 320 KB RAM is a luxury, not a necessity. Compile-time gating costs zero RAM, zero cycles in the hot path, and gives the strongest guarantee (the code is gone, not hidden behind a branch). If a future user needs runtime filtering, it can be added inside `Log::emit` without changing any caller.

**Alternatives considered:**
- *Runtime tag-level registry (`esp_log_level_set` does real work).* Doubles the size of `Log::emit` and burns ~1 KB of RAM on a level table. Not justified for an embedded LED controller.
- *Higher default `CORE_DEBUG_LEVEL` (e.g., 2 to keep some INFO quiet).* `INFO` (level 2) is the only level between `WARN` (3) and `DEBUG` (4), and "default INFO" is what we already have. No point.

### 4. Short single-word tags, one tag per file

**Decision:** Each `.cpp` file declares `static const char* TAG = "<shorttag>";` and uses it consistently. Tag list:

| File | TAG |
|------|-----|
| `main.cpp` | `main` |
| `Network.cpp`, `CaptivePortal.cpp` | `net` |
| `WebServerManager.cpp` | `http` |
| `Config.cpp` | `cfg` |
| `ShowController.cpp` | `ctrl` |
| `ShowFactory.cpp`, `show/*.cpp` | `show` |
| `TimerScheduler.cpp` | `timer` |
| `TouchController.cpp` | `touch` |
| `task/LedShow.cpp` | `led` |
| `strip/Base.cpp`, `strip/Layout.cpp` | `strip` |
| `OTAUpdater.cpp` | `ota` |

**Why:** Matches klimacontrol and ESP-IDF conventions. Short tags stay readable in fixed-width and grep well. One tag per file (not per function) keeps the file self-describing — you can grep `tag:strip` and find every strip-related line.

**Alternatives considered:**
- *Per-function tags (e.g., `net-ap`, `net-sta`, `net-ntp`).* Splits Network into 5+ tags. More precise but harder to reason about and inconsistent with "one module = one tag".
- *Longer descriptive tags (`network`, `config`, `showcontroller`).* Fits more meaning but wastes column space and breaks grep muscle memory from klimacontrol/ESP-IDF.

### 5. `OTA_LOG` folded into `ESP_LOG*`, `OTA_DEBUG_LOGGING` removed

**Decision:** The 87 `OTA_LOG(...)` call sites in `src/OTAUpdater.cpp` become `ESP_LOG*(...)` with `tag = "ota"`. The `[OTA]` prefix text disappears (the `ota` tag serves the same role). The `OTA_DEBUG_LOGGING` flag and `OTA_LOG` macro are removed from `src/OTAConfig.h`. The `OTA_LOG_MEMORY` flag is unaffected (it gates the memory-stat prints, not the log calls).

```diff
- #define OTA_LOG(fmt, ...) Serial.printf("[OTA] " fmt "\n", ##__VA_ARGS__)
- 
- OTA_LOG("HTTP open failed: %s", esp_err_to_name(err));
+ ESP_LOGW("ota", "HTTP open failed: %s", esp_err_to_name(err));
```

**Why:** Two logging systems in one codebase is confusing. The `OTA_LOG` macro is exactly the same shape as `ESP_LOGI` minus the level. Folding it in gives OTA logging the same level discipline, native-test compatibility, and sink-readiness as the rest of the codebase.

**Why remove `OTA_DEBUG_LOGGING`:** Its current default is `1` (line 91 of `OTAConfig.h`), which means today OTA logging is on. After the change, `CORE_DEBUG_LEVEL = 0` gates only `D/V`; OTA's `E/W/I` calls always compile. This is a *correctness* improvement: today an OTA error message can be silently compiled out by flipping one flag; under the new regime, errors are always visible. The behavior change is intentional.

**Alternatives considered:**
- *Keep `OTA_LOG` and `OTA_DEBUG_LOGGING` alongside `ESP_LOG*`.* Two systems, two configuration flags, two mental models. Worst of both worlds.
- *Keep `OTA_DEBUG_LOGGING` as a module-specific mute toggle that `#define`s `CORE_DEBUG_LEVEL`.* Preserves the knob but makes one module special-cased. The same knob is achievable today with `-DCORE_DEBUG_LEVEL=0` at build time.

### 6. Level discipline rule, applied during migration

**Decision:** During the file-by-file migration, each call site is assigned a level per the rule:

```
E  something is broken, the operator needs to know now
W  something is wrong but the device recovered / is recovering
I  state changes and lifecycle (boot, connect, switch show, save config)
D  per-iteration or periodic detail useful when chasing a bug
V  below D — used sparingly, never in this codebase to start
```

**Why:** Today every call is unconditional. The migration is the natural moment to classify. Sites that are clearly high-frequency and not error-path (NTP tick every 5 minutes, WiFi diagnostics dump, LED per-iteration render log, show init dump) become `D` and disappear from production serial output via `CORE_DEBUG_LEVEL = 0`. This is a *side benefit* of the change — the production log becomes much quieter without losing information.

**Where "D" applies in this codebase (representative, not exhaustive):**
- `Network.cpp:367-370` — NTP update every 5 min → `D`
- `Network.cpp:138-149` — WiFi diagnostics dump (10 lines) → `D`
- `task/LedShow.cpp:82` — per-iteration render log → `D`
- `show/ColorRanges.cpp:19-66` — init-time color/range dump → `D`
- `OTAUpdater.cpp` "OTA diag:" block (multiple lines) → `D`

Everything else defaults to `I`, with explicit `E`/`W` for error paths.

**Alternatives considered:**
- *Full audit of all 196 sites during migration.* Cleaner but doubles the PR size and conflates mechanical migration with subjective classification. The rule above + representative audit is enough; the rest is follow-up.
- *Audit in a separate follow-up change.* Defers the value and risks a second churn. The "some level review" requested by the proposal covers ~80% of the value at ~20% of the cost.

### 7. Native test compatibility via stubs (not Arduino `Serial` shim)

**Decision:** Below `#ifdef ARDUINO`, all five `ESP_LOG*` macros expand to empty `do {} while(0)` (and `esp_log_level_set` becomes `(void)0`). No `Serial` symbol is referenced. No stub header is needed in the native env.

**Why:** The existing native test build (`platformio.ini [env:native]`) includes `show/`, `Timer.cpp`, `color.cpp`, `strip/`, `support/` — meaning `ColorRanges.cpp` (which uses `Serial.printf` today) is already being compiled into the native test. This currently works only because no native test actually exercises `ColorRanges` (or because there's an unwritten assumption that `Serial` is provided by the test framework). The new stubs make the contract explicit: native tests compile without any logging side effects.

**Alternatives considered:**
- *Provide a `Serial` stub for native that writes to stdout.* Adds a file and changes build semantics. Stubs in the macro are simpler and remove the dependency entirely.
- *Keep `Serial.printf` for the few show files used in native tests, only convert the rest.* Splits the codebase between two log systems based on which files the test env compiles. Fragile.
