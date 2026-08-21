## Why

`platformio.ini` does not describe the firmware that gets built.

```
declared (platformio.ini:36-41)              actually resolved (.pio/libdeps)
─────────────────────────────────────        ──────────────────────────────────
me-no-dev/ESPAsyncWebServer.git       ─────▶  ESP32Async/ESPAsyncWebServer 3.6.0
me-no-dev/AsyncTCP.git                ─────▶  ESP32Async/AsyncTCP          3.3.2
bblanchon/ArduinoJson@^6.21.3         ─────▶  ArduinoJson                  7.4.3
                                              + AsyncTCP 3.4.10 (orphan, unused)
```

`me-no-dev` transferred both repositories into the `ESP32Async` organisation. GitHub keeps a redirect on a transferred repo, so `git clone me-no-dev/AsyncTCP.git` silently hands back the fork. The build has been running on the maintained fork for months without anyone deciding to migrate.

This is not a theory. The firmware already calls fork-only APIs that do not exist upstream:

```
src/WebServerManager.h:23     class AccessLogger : public AsyncMiddleware
src/WebServerManager.h:25     void run(..., ArMiddlewareNext next) override
src/WebServerManager.cpp:102  request->getResponse()
```

Three consequences follow:

- **The build is not reproducible.** A git URL with no ref pins nothing. The tree in `.pio/libdeps` is a snapshot of whatever the default branch held on 15 March 2026. A clean checkout on another machine today gets different code, and the day someone registers a new `me-no-dev/AsyncTCP` the redirect resolves somewhere else entirely.
- **Six minor versions of fixes are being skipped.** ESPAsyncWebServer is at **3.12.0**, AsyncTCP at **3.5.0**. The device runs 3.6.0 / 3.3.2. The gap includes a use-after-free fix in synchronous `abort`/`close` (#465), a crash on empty or out-of-range request params (#457), and request chunked-encoding support (#377).
- **`ArduinoJson@^6.21.3` resolved to 7.4.3.** The declared constraint is simply not what was installed, and `integrity.dat` records the spec as `^7.4.3` — the working tree has an uncommitted bump. **The tree does not build in this state**, and never has: `.pio/build` holds only `idedata.json`. Almost all of the 36 document sites survive on 7.4.3's deprecated `StaticJsonDocument` shim, but one does not — see below.

That shim is why the migration is nearly source-compatible, and it is worth reading:

```cpp
// ArduinoJson 7.4.3 — compatibility.hpp:63
template <size_t N>
class ARDUINOJSON_DEPRECATED("use JsonDocument instead") StaticJsonDocument
    : public JsonDocument {          // N is ignored entirely
 public:
  using JsonDocument::JsonDocument;
  size_t capacity() const { return N; }   // reports N, allocates elastically
};
```

Every one of the 35 `StaticJsonDocument<Config::JSON_DOC_*>` sites is *already* an elastic heap document. The six `JSON_DOC_*` constants in `Config.h:19-24` no longer control anything, and this comment already describes a bug that no longer exists:

The one site the shim does *not* carry is `ShowFactory`. Under v6, `StaticJsonDocument<N>` had a converting constructor from any document, so `createShow` parsing into a `JSON_DOC_LARGE` buffer (`ShowFactory.cpp:219`) and invoking a constructor declared to take `JSON_DOC_MEDIUM` (`ShowFactory.h:31`) compiled, via a silent re-copy. The v7 shim inherits only `JsonDocument`'s constructors, so sibling capacities are unrelated types and the call is a hard error:

```
src/ShowFactory.cpp:230: error: no match for call to
  '(std::function<...(const StaticJsonDocument<512>&)>) (StaticJsonDocument<1024>&)'
```

The declared-versus-passed mismatch was therefore not merely untidy; it is what stops the tree building. Retyping both signatures to `const JsonDocument&` is the fix, which is why that step leads the change rather than trailing it.


```cpp
// WebServerManager.cpp:165
// XLARGE, not LARGE: show_params is deep-copied from a MEDIUM parse
// buffer and ArduinoJson overflows silently, truncating the response.
```

Two live defects surfaced while mapping this, both in code the migration has to touch anyway:

**1. Every request body handler ignores everything after the first TCP segment.** The pattern appears 15 times:

```cpp
[this](AsyncWebServerRequest *request, uint8_t *data, size_t len,
       size_t index, [[maybe_unused]] size_t total) {
    if (index == 0) {
        StaticJsonDocument<Config::JSON_DOC_MEDIUM> doc;
        DeserializationError error = deserializeJson(doc, data, len);  // first chunk only
```

`total` is `[[maybe_unused]]` in 14 of the 15. A body larger than one segment (~1460 bytes) is parsed from its first fragment, fails deserialization, and returns 400. `POST /api/show` for a ColorRanges show with many colours and nested arrays, and `POST /api/presets` saving that show, are both realistically over the line. Raising the ArduinoJson capacity — which v7 already did — does not fix this, because the remaining bytes were never handed to the parser.

**2. `hasServedAnyRequest()` can never become true.** `markServedRequest()` is called from exactly one place, `AccessLogger::run` (`WebServerManager.cpp:89`), and the middleware is not installed:

```cpp
// WebServerManager.cpp:1468
// server.addMiddleware(&logging);
```

So `Network.cpp:377`'s `OTA_AUTO_CONFIRM_REQUIRE_REQUEST` gate reads a flag that is permanently `false`, and the access log never emits. One commented-out line, in the middleware machinery this change is already making load-bearing.

## What Changes

- **Dependencies are declared honestly and pinned.** Git URLs become registry specs: `esp32async/ESPAsyncWebServer@^3.12.0`, `esp32async/AsyncTCP@^3.5.0`, `bblanchon/ArduinoJson@^7.4.3`. The orphan AsyncTCP copy in `.pio/libdeps` goes away because nothing pulls a second one. The AsyncTCP jump (3.3.2 → 3.5.0) is the only change that puts genuinely new code on the device.
- **`StaticJsonDocument<N>` and `DynamicJsonDocument(n)` become `JsonDocument`** at all 36 sites, and the deprecated v6 accessors go with them: `containsKey()` → `is<T>()` (24 sites), `createNestedArray()`/`createNestedObject()` → `to<JsonArray>()`/`add<JsonObject>()` (12 sites). `Config::JSON_DOC_TINY/SMALL/MEDIUM/LARGE/XLARGE/OTA` are deleted — they no longer describe anything.
- **`-Werror=deprecated-declarations` is added to the ESP32 env** so the compiler enumerates the remaining sites and the codebase cannot silently regrow them.
- **`ShowFactory` stops naming a buffer size in its type.** `ShowConstructor` and `parseColors` take `const JsonDocument&`, so the 12 show lambdas no longer restate a capacity they never used. This also resolves the existing mismatch where the declared contract is `JSON_DOC_MEDIUM` (`ShowFactory.h:31`) but `createShow` passes a `JSON_DOC_LARGE` document (`ShowFactory.cpp:219`).
- **The 15 hand-rolled body handlers become `AsyncCallbackJsonWebHandler`,** which accumulates the full body across segments before parsing. This is what actually fixes defect 1, and it deletes the `index`/`len`/`total`/`_tempObject` bookkeeping from every POST and DELETE route.
- **The middleware is installed** and `AccessLogger` starts working, fixing defect 2.
- **`ShowFactory` joins the native build** so the JSON parameter path gets its first host-side test coverage.
- **The coverage pipeline is repaired** — added to scope after the work above ran into it. It had never produced a report: the generator was registered as a post-action on a target named `test`, which is a source directory here and never a build node, so it never fired; a duplicate copy of the same logic in `scripts/generate_coverage_manual.py` papered over it in CI, which then only *warned* when no report appeared. Separately, a stale `.gcda` from a recompiled object makes the coverage runtime segfault at exit after every test has passed, turning a green suite into `ERRORED`. One implementation replaces the two, stale data is dropped after each link, and report generation becomes an explicit `coverage` target rather than a hook that pretends.

Two behaviour changes follow from `AsyncCallbackJsonWebHandler` and are specified rather than absorbed silently:

```cpp
// AsyncJson.cpp — AsyncCallbackJsonWebHandler::canHandle
if (request->method() != HTTP_GET &&
    !request->contentType().equalsIgnoreCase(T_application_json))
  return false;                        // ← falls through to onNotFound → 404
```

- **`Content-Type: application/json` becomes mandatory** on every body-carrying request. All 13 body-carrying `fetch()` calls in `data/*.html` already send it — verified — so the web UI is unaffected. External clients that omitted it change from working to 404, and in AP mode to the captive-portal redirect.
- **Route matching becomes prefix-based**, not exact: `_uri != url && !url.startsWith(_uri + "/")`. `/api/presets` and `/api/presets/load` are both `POST` and both would match, so registration order becomes load-bearing where it previously was not.

## Capabilities

### New Capabilities
- `http-json-api`: Describes the transport-level contract every JSON endpoint honours — request framing across TCP segments, the `Content-Type` requirement, route matching and its ordering constraint, size limits, and the status codes and bodies returned for malformed or oversized input. No existing spec covers how a request body reaches a handler; the endpoint specs describe payloads, not framing.

### Modified Capabilities
- *(none)* — `web-ui-controls`, `settings-page`, `timer-scheduling` and `ota-update` describe payload shapes and UI behaviour, none of which change. Response bodies for successful requests are byte-identical.

## Impact

- `platformio.ini` — `lib_deps` rewritten to pinned registry specs; `-Werror=deprecated-declarations` added; `[env:native]` gains ArduinoJson and `+<ShowFactory.cpp>` in `build_src_filter`
- `src/Config.h` — `JSON_DOC_*` constants (lines 19-24) deleted
- `src/WebServerManager.{h,cpp}` — 34 `StaticJsonDocument` sites, 24 `containsKey`, 12 `createNested*`; 15 body handlers replaced by `AsyncCallbackJsonWebHandler`; `addMiddleware` uncommented. The largest part of the diff by far
- `src/ShowFactory.{h,cpp}` — `const JsonDocument&` in both signatures, 12 lambdas updated, the dead non-Arduino `#else` branch (`ShowFactory.cpp:233`) either made compilable or removed
- `src/OTAUpdater.cpp` — one `DynamicJsonDocument(Config::JSON_DOC_OTA)` at line 792
- `test/test_show_factory/` — extended to exercise the JSON parameter path natively
- `scripts/coverage_report.py` — **new**, the single coverage implementation; `scripts/coverage_post.py` — **new**, stale-`.gcda` cleanup and the `coverage` target; `scripts/coverage_link.py` reduced to link flags; `scripts/generate_coverage.py` and `scripts/generate_coverage_manual.py` — **deleted**
- `.github/workflows/test.yml`, `README.md`, `.gitignore` — coverage step now fails on a missing report, the "generated automatically" claim is gone, and the report artifacts are ignored
- `docs/SHOW_PARAMETERS.md:435` — the "~512 bytes during JSON parsing (StaticJsonDocument<512>)" note is now wrong

Risk is concentrated in the handler rewrite, not the dependency bump. The ESPAsyncWebServer 3.6.0 → 3.12.0 changelog's breaking changes are confined to the WebSocket layer and to `abort()` callback threading; this firmware uses no WebSockets and calls no `abort()`. Version 3.10.1 is marked **DO NOT USE** upstream and must not be pinned.

## Non-goals

- **Reverting to ArduinoJson 6.** The fork's bundled `AsyncJson.h` compiles against both, but v6 is in maintenance and the tree is already on 7.4.3 in practice.
- **Changing any request or response payload shape.** Success responses stay byte-identical; only framing, `Content-Type` handling and malformed-input status codes are in scope.
- **Adopting `AsyncJsonResponse` for GET routes.** The `serializeJson(doc, String)` → `request->send()` pattern works, allocates once, and rewriting it buys nothing this change needs.
- **WebSocket or SSE adoption.** Available in the fork, used by nothing here, and the polling UI is adequate.
- **A general request-validation layer.** Each handler keeps validating its own fields; only the parse-and-frame step is being centralised.
- **Bumping Adafruit NeoPixel (1.15.2 → 1.15.5) or NTPClient.** Unrelated to the async/JSON stack; separate change.
