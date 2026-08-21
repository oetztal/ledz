## 1. Pin the dependencies (no source changes)

- [x] 1.1 Confirm the migration is a no-op for the fork's breaking changes: `grep -rn "AsyncWebSocket\|AsyncEventSource\|->abort(" src` returns nothing. If any hit appears, stop and revisit design decision 2 before pinning
  - Clean: 0 hits for `AsyncWebSocket`, `AsyncEventSource`, `AsyncClient` and `->abort(`. The 8 `abort()` matches in `OTAUpdater.cpp` are `Update.abort()` from `<Update.h>` (ESP32 flash update), unrelated to AsyncTCP
- [x] 1.2 Replace the two git URLs and the ArduinoJson spec in `platformio.ini` `lib_deps` with `esp32async/ESPAsyncWebServer@^3.12.0`, `esp32async/AsyncTCP@^3.5.0`, `bblanchon/ArduinoJson@^7.4.3`. Keep `DNSServer`, NeoPixel and NTPClient untouched — they are out of scope
- [x] 1.3 Never pin below `3.11.0`: upstream marks **3.10.1 DO NOT USE**. Record that in a comment next to the spec so a future downgrade does not land there
- [x] 1.4 `rm -rf .pio/libdeps/adafruit_qtpy_esp32s3_nopsram` and reinstall. Verify exactly **one** AsyncTCP directory remains — the `AsyncTCP@src-<hash>` copy and the duplicate 3.4.10 must both be gone
  - One `AsyncTCP` directory. `integrity.dat` now matches `platformio.ini` exactly
- [x] 1.5 Record the resolved versions from `pio pkg list` here, so the pre-change state (webserver 3.6.0 / tcp 3.3.2 / json 7.4.3) and the post-change state are both in the history

  | package | before | after |
  |---|---|---|
  | ESPAsyncWebServer | 3.6.0 (git URL via redirect) | **3.12.0** |
  | AsyncTCP | 3.3.2 (git URL) + orphan 3.4.10 | **3.5.0** (single copy) |
  | ArduinoJson | 7.4.3 (declared `^6.21.3`) | 7.4.3 (declared `^7.4.3`) |
  | Adafruit NeoPixel | 1.15.2 | 1.15.5 — *unintended*, see below |
  | NTPClient | 3.2.1 | 3.2.1 |

  NeoPixel moved 1.15.2 → 1.15.5 because its unchanged `^1.15.2` spec re-resolved against a wiped cache. The proposal lists a NeoPixel bump as a non-goal; this was not an edit to its spec but it is a real change to the built firmware. Either accept it or pin `==1.15.2` — decided in task 6.6
- [x] 1.6a **Unplanned, blocking.** The build fails at `ShowFactory.cpp:230`: v7's shim has no cross-capacity conversion, so passing `StaticJsonDocument<1024>` to a `std::function` over `const StaticJsonDocument<512>&` is a hard error where v6 silently re-copied. Task 3.10 is pulled forward to here — it is the fix, not a tidy-up. See the "Revised during implementation" note in design decision 6
  - `ShowFactory.h:31` `ShowConstructor` and `:52` `parseColors` now take `const JsonDocument&`; 12 show lambdas in `ShowFactory.cpp` updated; stale comment at `:25` corrected
  - Deferred to section 3 as planned: `ShowFactory.cpp:219` and `:233` keep `StaticJsonDocument` (they compile — derived-to-base still binds), and the `JsonArrayConst` comment at `:32` is still task 3.9
  - Noted for section 6: `parseColors` is **declared at `ShowFactory.h:52` but never defined and never called**. Dead declaration; delete it rather than carry it
- [x] 1.6 Build `-e adafruit_qtpy_esp32s3_nopsram`. **`StaticJsonDocument` is still in the sources at this point and that is intended** — the v7 shim makes it compile. Deprecation warnings are expected and must not be errors yet
  - SUCCESS in 12.5 s after 1.6a. 4 deprecation warnings remain, all `StaticJsonDocument`, none fatal — exactly the state section 3 clears
  - Do not judge the build by `pio run | tail`: the pipe masks the exit code, and the first attempt here reported "exit 0" on a failed build. Redirect to a file and check `$?`
- [ ] 1.7 Flash and smoke-test on hardware before touching any source: control page renders, a show applies, `GET /api/status` returns full JSON, settings and timers pages load and save. This isolates "did the library bump break anything?" from every later step
- [ ] 1.8 Commit. This must be a standalone commit so it can be bisected against

## 2. Native test net for the JSON parameter path

- [x] 2.1 Add `bblanchon/ArduinoJson@^7.4.3` to `[env:native]` `lib_deps` — the line is already present, commented out at `platformio.ini:24`, pinned at v6. Uncomment and bump
  - The commented `skaygin/arduino-native` line was dropped rather than revived; nothing needs it
- [x] 2.2 Add `+<ShowFactory.cpp>` to the native `build_src_filter` (`platformio.ini:14`)
- [x] 2.3 Remove the `#ifdef ARDUINO` guard around `#include <ArduinoJson.h>` in `ShowFactory.h` — the header's public interface names an ArduinoJson type unconditionally, so the guard was never coherent (design decision 9)
  - `<Arduino.h>` stays guarded; only the ArduinoJson include is unconditional. The now-redundant guard in `ShowFactory.cpp` was dropped too
- [x] 2.4 Delete the `#else` branch at `ShowFactory.cpp:233`. It instantiates `StaticJsonDocument` in a translation unit where the type was not declared and has never compiled. `createShow` keeps one code path
  - Also deleted `ShowFactory::parseColors` (declared at `ShowFactory.h:52`, never defined, never called)
- [x] 2.5 Resolve whatever `ESP_LOGW`/`Arduino.h` dependencies the native build now surfaces in `ShowFactory.cpp`, matching how `src/support/` already handles it
  - `Log.h` already no-ops `ESP_LOG*` natively, so all 9 `#ifdef ARDUINO` guards around logging were redundant and were removed. `ShowFactory.cpp` now has zero preprocessor guards
  - **Second never-compiled branch found.** The MorseCode `#else` declared `const char *message` and then called `message.c_str()`. Replaced with an unguarded `const char *message = doc["message"] | "HELLO"`; `Show::MorseCode` takes `const std::string&` and copies, so the document's pointer is safe for the duration of the call
- [x] 2.6 Extend `test/test_show_factory/` to build a `JsonDocument` directly and call `createShow(name, paramsJson)` for each registered show: assert defaults are applied for an empty document, that supplied values override them, and that a malformed parameter string falls back to defaults rather than failing
  - The file previously tested a *locally re-implemented* `parseColors` whose colour-cycling logic exists nowhere in production. Replaced with 14 tests driving the real factory
- [x] 2.7 ~~Add a `parseColors` test~~ — **not possible as written**: `ShowFactory::parseColors` has no definition and no caller. Covered the equivalent ground against `createShow` instead: absent `colors` key (warm-white default), one colour, two colours, 24 colours, and entries that are too short or not arrays at all
- [x] 2.8 Add a case for the `ranges` path in the Solid show, which is the one that motivated the `JSON_DOC_LARGE` parse buffer at `ShowFactory.cpp:219`
  - `ranges:[30]` moves the boundary to pixel 3, and a 24-colour payload round-trips with every colour intact
- [x] 2.9 `pio test -e native` passes. Record the test count before and after
  - **109 → 113** across 10 suites. `test_show_factory` went from 10 fictional cases to 14 real ones
  - Shows expose no parameter accessors, so assertions are behavioural: render to a `MockStrip` and read pixels back. `ColorRanges` blends over a wall-clock 2 s that a test cannot fast-forward, and `ColorRanges::execute` steps only while `!isComplete()` — so the strip **never lands exactly on its target**, and sleeping past 2 s freezes it at black. The tests settle to 1500 ms and calibrate the residual scale factor from a pure-red probe
  - **Repo papercut, pre-existing:** a stale `.gcda` from a previous build is incompatible with a recompiled test binary, and the `--coverage` runtime *segfaults* at exit (`GCDAProfiling.c:170`) instead of erroring. Tests all report PASSED and the suite still reports ERRORED. Originally logged as out of scope; **pulled in on request — see section 7**

## 3. JsonDocument sweep

- [x] 3.1 Add `-Werror=deprecated-declarations` to `[env:adafruit_qtpy_esp32s3_nopsram]` `build_flags`. Build once and capture the full error list — this is the authoritative worklist for this section, not the counts below
  - **The flag alone is a no-op.** espressif32 appends its own `-Wno-error=deprecated-declarations` *after* `build_flags`, and GCC takes the last one; the build passed with all 35 sites still present. `build_unflags` must drop the platform's override too
  - Real worklist once the flag bit: **72 errors** — 67 `WebServerManager.cpp`, 3 `ShowFactory.cpp`, 1 `OTAUpdater.cpp`:

    | replacement | count |
    |---|---|
    | `use JsonDocument instead` | 35 |
    | `use doc["key"].is<T>() instead` | 25 |
    | `use doc[key].to<JsonArray>() instead` | 5 |
    | `use add<JsonObject>() instead` | 4 |
    | `use doc[key].to<JsonObject>() instead` | 3 |
- [x] 3.2 `src/WebServerManager.cpp`: 34 `StaticJsonDocument<Config::JSON_DOC_*>` → `JsonDocument`
- [x] 3.3 `src/OTAUpdater.cpp:792`: `DynamicJsonDocument doc(Config::JSON_DOC_OTA)` → `JsonDocument doc`
- [x] 3.4 `src/ShowFactory.cpp:219` and `:233`: same, in the surviving `createShow` path
  - `:233` no longer exists; the dead `#else` went in task 2.4
- [x] 3.5 ~~`doc.containsKey("x")` → `doc["x"].is<T>()`~~ — **used `!doc["x"].isNull()` instead**, 25 sites (23 `WebServerManager.cpp`, 2 `ShowFactory.cpp`)
  - `is<T>()` is what the deprecation message suggests, but it is not behaviour-preserving and this section's contract is that it is. Every one of these is a *presence* guard, and the two differ on a wrong-typed value: today `POST /api/settings/device {"num_pixels":"abc"}` enters the branch, reads 0, and answers `400 must be 1-1000`. Under `is<uint16_t>()` it would skip the branch and silently accept the request. `!isNull()` keeps the existing diagnostic
  - Divergence from `containsKey` is limited to an explicit JSON `null` value, which no client here sends
  - Tightening these to `is<T>()` is a defensible follow-up, but it is a deliberate API change and belongs in its own change
- [x] 3.6 12 × `createNestedArray(k)` → `doc[k].to<JsonArray>()`, `createNestedObject()` on an array → `arr.add<JsonObject>()`
  - 5 keyed arrays, 3 keyed objects, 4 array-appends — matching the compiler's counts exactly
- [x] 3.7 Delete `Config.h:19-24` — all six `JSON_DOC_*` constants. `grep -rn JSON_DOC_ src` must return nothing
- [x] 3.8 Delete the now-false comment at `WebServerManager.cpp:164-166` explaining why XLARGE was chosen over LARGE
  - A second stale capacity comment was found and removed at the timezone route (`JSON_DOC_SMALL rather than TINY`, ~`:1141`)
- [x] 3.9 Re-check `ShowFactory.cpp:32`'s note that `JsonArrayConst` is required rather than `JsonArray`. That was a v6 constraint; verify against the v7 behaviour and update or delete the comment accordingly, with task 2.8 as the check
  - Still required, but not for the stated reason: `doc` is a `const JsonDocument&`, so subscripts are `JsonVariantConst` and convert only to the const views. Nothing to do with `StaticJsonDocument`. Comment corrected
- [x] 3.10 `ShowFactory.h:31` and `:52`: `ShowConstructor` and `parseColors` take `const JsonDocument&`. Update the 12 show lambdas in `ShowFactory.cpp`. This also removes the existing MEDIUM-declared / LARGE-passed mismatch
  - Done early as task 1.6a — it was blocking the build, not a cleanup
- [x] 3.11 Update `docs/SHOW_PARAMETERS.md:435` — "~512 bytes during JSON parsing (StaticJsonDocument<512>)" no longer describes anything
  - Rewritten as a heap note: elastic allocation, no capacity to tune, no silent truncation
- [x] 3.12 Build clean with no deprecation errors; `pio test -e native` still passes
  - ESP32 build: SUCCESS, **zero errors and zero warnings** with `-Werror=deprecated-declarations` active. Native: **113/113**
- [ ] 3.13 Flash and re-run the task 1.7 smoke test. Diff `GET /api/status`, `GET /api/shows`, `GET /api/timers` and `GET /api/about` against captures taken at step 1.7 — they must be byte-identical
- [ ] 3.14 Commit separately from section 4

## 4. Convert the body handlers

- [x] 4.1 Add `#include <AsyncJson.h>` to `WebServerManager.cpp`. Confirm `ASYNC_JSON_SUPPORT` resolves to 1 and that the v7 branch of `AsyncCallbackJsonWebHandler` is the one compiled (the v6 branch takes a `maxJsonBufferSize` constructor argument; the v7 one does not)
  - v7 branch confirmed: the two-argument constructor compiles, so `ARDUINOJSON_VERSION_MAJOR != 6`
- [x] 4.2 Verify the class's virtual signatures in the pinned version still match 3.6.0's — `canHandle` gained a `const` qualifier at some point in the 3.x line, and the conversion depends on the class shape, not on subclassing it
  - **They do not, and it is an improvement.** 3.12.0 takes an `AsyncURIMatcher` rather than a `const String&`. A plain string still gives the old prefix behaviour, but `AsyncURIMatcher::exact()` gives `^uri$`. All 15 routes use `exact()`, which deletes the ordering hazard instead of documenting it — see the "Superseded during implementation" note in design decision 5
  - `canHandle`/`handleRequest`/`handleBody` are all still `final`, so design decision 3 (accept the bare 400/413) stands unchanged
- [x] 4.3 Convert the 15 body-carrying routes, one commit each or in small batches. Each becomes an `AsyncCallbackJsonWebHandler` registered with `server.addHandler(...)`, taking `(AsyncWebServerRequest*, JsonVariant&)`; the `index == 0` guard, the `data`/`len`/`total` parameters and the local document all disappear:
  - Post-conversion counts: 0 `if (index == 0)`, 0 raw body lambdas, 15 handlers, 15 `setMethod()`
  - **`total` was `[[maybe_unused]]` in all 15, including `handleWiFiConfig`** — so every route, not just 14, silently discarded everything after the first segment. `handleWiFiConfig` now takes `(AsyncWebServerRequest*, JsonVariant&)`; its declaration in `WebServerManager.h:72` changed to match
  - The parse prologue was not always the first statement in the block: four timer routes and the touch route null-check their subsystem first. Stripping it by position would have been wrong

  - [x] 4.3.1 `POST /api/wifi` (`:139`, setup mode — delegates to `handleWiFiConfig`, the one handler that currently uses `total`)
  - [x] 4.3.2 `POST /api/show` (`:232`)
  - [x] 4.3.3 `POST /api/brightness` (`:272`)
  - [x] 4.3.4 `POST /api/layout` (`:302`)
  - [x] 4.3.5 `POST /api/presets/load` (`:387`) — **register before 4.3.6**
  - [x] 4.3.6 `POST /api/presets` (`:455`)
  - [x] 4.3.7 `DELETE /api/presets` (`:533`) — needs `setMethod(HTTP_DELETE)`
  - [x] 4.3.8 `POST /api/settings/wifi` (`:591`)
  - [x] 4.3.9 `POST /api/settings/device-name` (`:638`)
  - [x] 4.3.10 `POST /api/settings/device` (`:675`)
  - [x] 4.3.11 `POST /api/timers/countdown` (`:915`)
  - [x] 4.3.12 `POST /api/timers/alarm` (`:999`)
  - [x] 4.3.13 `DELETE /api/timers` (`:1083`) — needs `setMethod(HTTP_DELETE)`
  - [x] 4.3.14 `POST /api/timers/timezone` (`:1129`)
  - [x] 4.3.15 `POST /api/touch` (`:1210`)

- [x] 4.4 The default `_method` is `HTTP_GET | HTTP_POST | HTTP_PUT | HTTP_PATCH`. Call `setMethod()` on **every** handler to state the method exactly rather than inheriting a set wider than the route accepts
  - 15 handlers, 15 `setMethod()` calls, including `HTTP_DELETE` for the two delete routes
- [x] 4.5 ~~Preserve `/api/presets/load` before `/api/presets`~~ — **moot.** `AsyncURIMatcher::exact()` makes registration order irrelevant, so there is no load-bearing order to comment on. The spec requirement changed from "prefix-based, most specific first" to "matches its URI exactly"
- [x] 4.6 Leave the four bodyless POSTs as plain `server.on()`: `/api/restart` (`:584`), `/api/settings/factory-reset` (`:772`), `/api/ota/update` (`:1267`), `/api/ota/confirm` (`:1363`). Two of them are called without a `Content-Type` header and would stop matching
  - All four verified untouched
- [x] 4.7 Remove `JSON_RESPONSE_ERROR_INVALID_JSON` if the conversion leaves it unreferenced. Malformed input is now answered by the library with a bare 400 (design decision 3)
  - Left with zero references after the sweep; removed
- [x] 4.8 Uncomment `server.addMiddleware(&logging)` at `:1468`
- [x] 4.9 Fix `AccessLogger::run`: `ip` and `url` are `const char*` taken from destroyed `String` temporaries and dangle before `snprintf` reads them. Hold the `String`s in named locals, or format directly from them. This code has never run and starts running at 4.8
  - Now `const String ip` / `const String url` with `.c_str()` at the `snprintf` call. Also dropped the unused `Print *_out = &Serial;` local
- [ ] 4.10 Verify `hasServedAnyRequest()` now becomes true after any request, and that `Network.cpp:377`'s auto-confirm gate can be satisfied
  - **Blocked on hardware** — verified statically only: the middleware is installed and `WebRequest.cpp:159` wraps whichever handler the router selected, so `addHandler()` routes are covered. Runtime confirmation belongs to task 5.9

## 5. Verify on hardware

> **Archive deferred (2026-08-21).** `/opsx:archive` was run and declined: the
> delta spec asserts two things no one has yet observed on a device — byte-identical
> `GET` responses (5.2) and the multi-segment body fix (5.3), which is the entire
> point of section 4. The spec stays inside the change and `openspec/specs/` keeps
> describing only observed behaviour until this section passes.


- [ ] 5.1 Full UI pass on every page: control (show selection, each parameterised show, brightness, layout, presets save/load/delete), settings (device name, hardware, WiFi, touch, factory reset), timers (countdown, alarm, cancel, timezone), about, OTA check
- [ ] 5.2 Re-diff every `GET` response against the step 1.7 captures. Byte-identical
- [ ] 5.3 Multi-segment body test — the whole point of section 4. Apply a ColorRanges show with enough colours to push the `POST /api/show` body over ~1460 bytes, confirm it applies fully, then save it as a preset and reload it. Confirm this same payload fails on the pre-change firmware, so the fix is demonstrated rather than assumed
- [ ] 5.4 `curl -X POST http://<device>/api/brightness -d '{"value":100}'` **without** `Content-Type` → 404 in station mode, captive-portal redirect in AP mode. Expected per the spec; confirm it is what actually happens
- [ ] 5.5 `curl` the same with `-H 'Content-Type: application/json'` → normal response
- [ ] 5.6 Malformed body with the correct header → bare 400, empty body
- [ ] 5.7 Confirm the UI degrades acceptably against an empty 400 body: the 14 `result.error` sites fall into their `catch` and show a generic message rather than throwing uncaught (design decision 3)
- [ ] 5.8 AP / captive-portal mode: `POST /api/wifi` from the setup page still provisions correctly, and the portal redirect still fires for unrelated paths
- [ ] 5.9 Access log lines appear on serial with a plausible IP, URL, method and status
- [ ] 5.10 OTA check and update against the real endpoint — `OTAUpdater.cpp` parses the manifest with what is now an elastic document
- [ ] 5.11 Leave the device up for an extended run exercising the polling UI, and watch free heap. v7 documents are heap-allocated on the AsyncTCP task and this board has no PSRAM; a downward trend is the signal to care about

## 6. Close out

- [x] 6.1 `grep -rn "StaticJsonDocument\|DynamicJsonDocument\|JSON_DOC_\|containsKey\|createNested" src` returns nothing
- [x] 6.2 `grep -rn "me-no-dev" platformio.ini` returns nothing
  - One deliberate hit remains: the comment above `lib_deps` explaining *why* the git URLs were replaced. The dependency specs themselves are clean
- [ ] 6.3 Resolve the `WIP on (no branch): separate webserver modes` stash — it touches `src/WebServerManager.cpp` (5 lines) and `src/Network.cpp` (86 lines). Rebase it onto the result or drop it; it will not apply cleanly after section 4
- [x] 6.4 Decide whether `-Werror=deprecated-declarations` stays permanently. Keeping it stops the codebase regrowing v6 spellings; it also means an upstream deprecation in any dependency breaks the build
  - **Kept.** It is the only thing standing between this codebase and a slow drift back to v6 spellings, and it caught all 72 sites without a single manual grep
  - It requires `build_unflags` to keep dropping espressif32's `-Wno-error=deprecated-declarations`; without that line the flag is silently inert, which is worse than not having it. Both must move together
  - Accepted risk: a future dependency bump that deprecates something will fail the build rather than warn. The escape hatch is deleting the `build_unflags` line, not the `build_flags` one
- [x] 6.5 Note in the change record that `_maxContentLength` is left at the library default of 16384 and that a deliberate cap is a separate decision (design decision 7)
  - No `setMaxContentLength()` call on any of the 15 handlers, so all use the library's 16384-byte default
  - This is a change in *shape* but not in exposure: the old handlers had no cap at all, and the new ones answer 413 above 16 KB. On a no-PSRAM board the `malloc(total)` is still driven by a client-supplied `Content-Length`, exactly as the old accumulation was
  - A deliberate cap should be one number chosen against the largest legitimate body (the ColorRanges `/api/show` payload) and applied uniformly — separate change
- [ ] 6.6 Decide on the incidental Adafruit NeoPixel 1.15.2 → 1.15.5 bump recorded in task 1.5. Its spec was not edited by this change; the cache wipe simply re-resolved `^1.15.2`. Accept it, or pin the exact version to keep this change's blast radius to the async/JSON stack

## 7. Fix the coverage implementation

Added on request after section 2 surfaced the stale-`.gcda` crash. Investigating it showed the crash was the smaller of two problems.

- [x] 7.1 Establish what the pipeline actually produced: **nothing**. No `coverage.info`, no `coverage_report/`, ever. `generate_coverage.py` registered `env.AddPostAction("test", ...)` against a target named `test`, which is a source *directory* in this repo and never a build node, so the action never fired
- [x] 7.2 Find the second implementation: `scripts/generate_coverage_manual.py`, a near-verbatim copy of the same logic carrying the same bugs, invoked from `.github/workflows/test.yml:57` as a workaround for 7.1. CI then checked `if [ ! -d coverage_report ]` and printed a *warning* — so a report that had never once been produced passed CI for the project's whole history
- [x] 7.3 Diagnose the crash: a `.gcda` accumulates counters and is merged into at exit. Once its source is edited the arc layout no longer matches, and LLVM's runtime does not fail cleanly — it emits `cannot merge previous GCDA file: corrupt arc tag` (551 times in one run here) and segfaults in `GCDAProfiling.c:170` *after* every test has reported PASSED. The suite shows ERRORED with no stated cause
- [x] 7.4 Establish why the fix has to be a `post:` script. `AddPostAction("$PROGPATH", ...)` and `AddCustomTarget` both need the program node the main build script creates; registered from the existing `pre:` script they attach to nothing and fail silently — the same failure mode as 7.1. Verified empirically: `$PROGPATH` substitutes correctly from `pre:`, but the action never runs
- [x] 7.5 `scripts/coverage_link.py` reduced to link flags only (still `pre:`, because those must precede the program node)
- [x] 7.6 New `scripts/coverage_post.py` (`post:`): drops any `.gcda` whose `.gcno` is newer, then registers the `coverage` target
  - The mtime rule is what preserves multi-suite reports: a fresh `.gcno` means that object was just recompiled and its data is stale, while an older `.gcno` means the data came from an earlier test binary in the same run and must be kept
  - Verified by instrumentation: across one `pio test` run the hook fired **10 times** (once per test binary), removed **1** file — the edited suite — and **0** in the other nine. Corruption messages went 551 → 0
- [x] 7.7 New `scripts/coverage_report.py` — the single implementation, usable as `python3 scripts/coverage_report.py` or `pio run -e native -t coverage`. Both duplicates deleted
  - Report generation cannot be automatic: PlatformIO builds and runs each test directory in turn, so no SCons action exists after the last binary *runs*. An explicit target is honest where the old post-action pretended
  - `--gcov-tool gcov` resolves to Apple's llvm-cov shim on macOS and gcc's on Linux, each correct for its own toolchain; `GCOV` overrides
  - lcov 2.x rejects data over inlined header destructors and `__cxx_global_var_init` entries with no line number. The ignore list is version-gated: the broad set on lcov ≥ 2, the old three on lcov 1.x
  - Filtering is `--extract src/*` (allowlist) then `--remove src/generated/*`, so new toolchain paths cannot drift into the report the way they can through a denylist. Matches `sonar-project.properties`' exclusions
  - Exits non-zero on any real failure, including "no `.gcda` — run the tests first"
- [x] 7.8 `.github/workflows/test.yml`: calls `scripts/coverage_report.py` in its own step and asserts `coverage_report/index.html` exists, so a broken report fails the job instead of warning
- [x] 7.9 `README.md`: removed the false claim that the report "is automatically generated after running tests"; documented the two-step flow and the stale-data cleanup
- [x] 7.10 `.gitignore`: added `coverage.info`, `coverage_filtered.info`, `coverage_report/`, none of which were ignored
- [x] 7.11 End-to-end verification from a wiped `.pio/build/native`

    | check | result |
    |---|---|
    | `pio test -e native` | 113/113 |
    | `python3 scripts/coverage_report.py` (CI path) | exit 0 |
    | `pio run -e native -t coverage` | exit 0 |
    | report | **38 files, 62.5% lines (699/1118), 66.2% functions (139/210)** |
    | no-`.gcda` failure path | exit 1 with an actionable message |
    | ESP32 build | SUCCESS, 0 warnings |
