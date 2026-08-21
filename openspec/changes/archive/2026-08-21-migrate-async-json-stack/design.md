## Context

Three libraries and one source file carry almost all of this change. `WebServerManager.cpp` is 1530 lines and holds 35 HTTP routes, 34 of the 36 JSON document sites, and all 15 request-body handlers. Everything else is small.

The current request path, for a body-carrying route:

```
  browser                    AsyncTCP task                      handler lambda
  ───────                    ────────────                       ──────────────
  POST /api/show
  Content-Type: json
  {"name":"Solid", ...}
        │
        ├─ segment 1 ──▶  onBody(data, len, index=0, total=N) ──▶ if (index == 0)
        │                                                            deserializeJson(doc, data, len)
        │                                                            validate, act, send()
        │
        └─ segment 2 ──▶  onBody(data, len, index=1460, ...)  ──▶ if (index == 0)  ✗ dropped
                                                                       │
                          onRequest(request) ─────────────────────────┘ (empty, body already answered)
```

For bodies under one TCP segment — which is all of them during normal UI use — this is correct. Above ~1460 bytes the parser sees a truncated fragment, `deserializeJson` returns `IncompleteInput`, and the route answers 400. `total` is `[[maybe_unused]]` in 14 of the 15 handlers, so the information needed to do better is present and discarded.

`AsyncCallbackJsonWebHandler` implements the missing half:

```cpp
// AsyncJson.cpp
void AsyncCallbackJsonWebHandler::handleBody(req, data, len, index, total) {
  _contentLength = total;
  if (total > 0 && req->_tempObject == NULL && total < _maxContentLength)
    req->_tempObject = malloc(total);
  if (req->_tempObject != NULL)
    memcpy((uint8_t*)(req->_tempObject) + index, data, len);   // reassemble
}

void AsyncCallbackJsonWebHandler::handleRequest(req) {
  ...
  JsonDocument jsonBuffer;
  DeserializationError error = deserializeJson(jsonBuffer, (uint8_t*)(req->_tempObject));
  if (!error) { _onRequest(req, jsonBuffer.as<JsonVariant>()); return; }
  req->send(_contentLength > _maxContentLength ? 413 : 400);
}
```

The document is a v7 `JsonDocument`, so it is elastic and the handler receives a `JsonVariant&` — the capacity question disappears at the same time as the framing one.

The three dependencies, as they stand:

```
  ┌──────────────────────┬──────────┬──────────┬────────────────────────────────┐
  │ package              │ building │ newest   │ how it is declared             │
  ├──────────────────────┼──────────┼──────────┼────────────────────────────────┤
  │ ESPAsyncWebServer    │ 3.6.0    │ 3.12.0   │ git URL, no ref, via redirect  │
  │ AsyncTCP             │ 3.3.2    │ 3.5.0    │ git URL, no ref, via redirect  │
  │ ArduinoJson          │ 7.4.3    │ 7.4.3    │ ^6.21.3 (does not match)       │
  └──────────────────────┴──────────┴──────────┴────────────────────────────────┘
```

`.pio/libdeps` also holds a second, unused AsyncTCP 3.4.10, pulled in as ESPAsyncWebServer's declared registry dependency. `idedata.json` shows the LDF resolved `#include <AsyncTCP.h>` to the git-URL copy (3.3.2), so the registry copy is dead weight rather than a symbol collision — but only by resolution order.

## Decision 1 — Pin registry specs with a caret, not git URLs and not exact versions

Git URLs without a ref pin nothing, and these two in particular resolve through an org-transfer redirect that the project does not control. Registry specs are what the fork publishes to and what its own `library.json` uses to express its AsyncTCP dependency.

```
esp32async/ESPAsyncWebServer @ ^3.12.0
esp32async/AsyncTCP          @ ^3.5.0
bblanchon/ArduinoJson        @ ^7.4.3
```

Caret rather than exact: this project has no lockfile, and the fork ships fixes at a high cadence within a minor. Exact pins would freeze the codebase into the same staleness the change exists to remove, just legibly this time. The caret bounds the risk to the major, which is where the fork places its breaking changes.

Declaring AsyncTCP explicitly — rather than letting ESPAsyncWebServer pull it — keeps the version visible in `platformio.ini` and prevents a second copy from appearing under a `@src-<hash>` directory again.

**3.10.1 is marked DO NOT USE upstream.** `^3.12.0` cannot resolve to it, but the constraint must never be relaxed below `3.11.0`.

## Decision 2 — Take the AsyncTCP jump in this change, and verify it by absence

3.3.2 → 3.5.0 is the only part of this migration that puts genuinely new machine code on the device; everything else is source-level. The temptation is to split it out.

Against splitting: ESPAsyncWebServer 3.12.0 *requires* AsyncTCP 3.5.0 (`library.json` dependency, and the 3.12.0 release notes call it out). Pinning the web server without the TCP layer produces a combination upstream does not test.

The 3.6.0 → 3.12.0 breaking changes, from the release notes, are exactly two:

| Release | Breaking change | Applies here? |
|---|---|---|
| 3.12.0 | `abort()` now runs `onDisconnect`/`onError` in the caller's task, not the async_tcp task | **No** — no `abort()` call anywhere in `src/` |
| 3.12.0 | Major WebSocket refactor (torn frames, use-after-free, ack computation) | **No** — no `AsyncWebSocket` |
| 3.11.0 | `CloseClientOnQueueFull` now defaults to false | **No** — WebSocket-only setting |
| 3.10.0 | WS_TEXT frames no longer require a caller-written null terminator | **No** — WebSocket-only |

The verification is therefore a grep, not a test: confirm `AsyncWebSocket`, `AsyncEventSource` and `->abort(` appear nowhere in `src/`. If that holds, the version jump is a pure bugfix intake, including the use-after-free fix in synchronous `abort`/`close` (#465) that the request teardown path benefits from regardless.

## Decision 3 — Accept the bare `400`/`413` body rather than fork the handler

`AsyncCallbackJsonWebHandler` answers malformed input with `request->send(400)` — status only, empty body. The existing handlers answer with an envelope:

```cpp
static const char* JSON_RESPONSE_ERROR_INVALID_JSON = R"({"success":false,"error":"Invalid JSON"})";
```

Fourteen call sites in `data/*.html` do `await response.json()` and read `result.error`:

```js
// timers.html:344
alert('Failed to set timer: ' + (result.error || 'Unknown error'));
```

Against an empty body, `response.json()` throws and the surrounding `try` produces its generic message instead. Degraded, not broken.

Overriding this is not available cheaply. All three virtuals are sealed:

```cpp
bool canHandle(AsyncWebServerRequest*) const override final;
void handleRequest(AsyncWebServerRequest*) override final;
void handleBody(...) override final;
```

So the options are:

| Option | Cost | Verdict |
|---|---|---|
| Accept bare 400/413 | zero | **chosen** |
| Copy the ~45-line class into `src/` and own it | a vendored fork to maintain across upstream bumps | rejected |
| Wrap in an outer `AsyncWebHandler` that re-answers | duplicates the reassembly the class exists to provide | rejected |

The deciding argument is reachability: the UI builds every body with `JSON.stringify`, so it cannot emit malformed JSON. The bare 400 is reachable only by a hand-rolled client, which reads the status code. A vendored copy of an upstream class, by contrast, is a permanent tax paid on every future bump — the exact failure mode this change exists to end.

Application-level failures (`{"success":false,"error":"Timer index out of range"}`) are unaffected: those are sent from inside the callback, which still owns its own responses.

## Decision 4 — Make the `Content-Type` requirement explicit in the spec

```cpp
// AsyncJson.cpp — canHandle
if (request->method() != HTTP_GET &&
    !request->contentType().equalsIgnoreCase(T_application_json))
  return false;
```

Returning `false` from `canHandle` does not produce a 415. The router simply keeps looking, finds nothing, and falls to `onNotFound` — which in AP mode is the captive-portal redirect (`WebServerManager.cpp:1501`, `:1520`). A client that omits the header gets a 302 to the setup page.

All 13 body-carrying `fetch()` calls in `data/` already send `'Content-Type': 'application/json'` — verified across `control.html`, `settings.html`, `timers.html`, `config.html`, including both `DELETE` bodies. The two `fetch()` calls that omit it, `/api/settings/factory-reset` (`settings.html:540`) and `/api/ota/update` (`settings.html:723`), send no body and keep their plain `server.on()` handlers.

This is nonetheless a real narrowing of the HTTP contract for anything driving the device by script, and it fails in a confusing way (302, not 415). It belongs in the spec as a stated requirement rather than as an implementation detail.

## Decision 5 — Preserve registration order, and prove the one collision is handled

`canHandle` matches by prefix:

```cpp
if (_uri.length() && (_uri != request->url() && !request->url().startsWith(_uri + "/")))
  return false;
```

Enumerating every pair of body-carrying routes that share a method:

```
  /api/presets          POST    ──┐ "/api/presets/load".startsWith("/api/presets/")  ✗ COLLIDES
  /api/presets/load     POST    ──┘

  /api/settings/device      POST  ──┐ "/api/settings/device-name"
  /api/settings/device-name POST  ──┘   .startsWith("/api/settings/device/")  → false  ✓ safe

  /api/timers           DELETE  ──┐ different methods from /countdown, /alarm,
  /api/timers/*         POST    ──┘   /timezone                                ✓ safe
```

The trailing `/` in `_uri + "/"` saves the `device` / `device-name` pair. It does not save `presets` / `presets/load`, which are both POST. Today the current code registers `/api/presets/load` at line 387 and `/api/presets` at line 455 — the specific route first, so it already wins by accident of ordering.

Rather than depend on that accident surviving a 1500-line refactor, the conversion registers the more specific URI first **and** the spec states the ordering requirement, so the constraint is checkable rather than folklore.

**Superseded during implementation.** This analysis was done against 3.6.0. Version 3.12.0 replaces the raw `String _uri` with an `AsyncURIMatcher`, and a plain string still yields the same `BackwardCompatible` (`^{uri}(/.*)?$`) behaviour analysed above — but the class now offers explicit factories:

```cpp
AsyncURIMatcher::exact("/api/presets")   // ^/api/presets$
AsyncURIMatcher::prefix(...)  dir(...)  ext(...)  regex(...)
```

All 15 converted routes use `AsyncURIMatcher::exact(...)`, which removes the collision at its source rather than working around it. Registration order stops being load-bearing, so the ordering requirement is gone from the spec and replaced with an exact-match requirement — a stronger and directly testable property. Each handler additionally calls `setMethod()` explicitly, because the constructor's default is `HTTP_GET | HTTP_POST | HTTP_PUT | HTTP_PATCH`, wider than any route here implements.

The four bodyless `POST` routes keep plain `server.on()` registration and therefore keep the old prefix behaviour. That is pre-existing and out of scope.

## Decision 6 — Sequence the work so each step is independently revertible

The change is four mechanically distinct edits with very different risk profiles. Landing them as one undifferentiated diff would make a bisect useless.

```
  1. Pin dependencies            platformio.ini + the ShowFactory signature fix
     ──────────────────────      (see below). Build must succeed with the other
            │                    35 StaticJsonDocument sites intact (v7 shim).
            ▼
  2. Native test net             ShowFactory into the native build + JSON tests.
     ──────────────────────      Pure addition. Gives steps 3-4 a safety net
            │                    they otherwise would not have.
            ▼
  3. JsonDocument sweep          Mechanical, compiler-enumerated via
     ──────────────────────      -Werror=deprecated-declarations. Zero
            │                    behaviour change by construction.
            ▼
  4. Handler conversion          The only step that changes request handling.
     ──────────────────────      Isolated, so a revert costs nothing already paid.
```

Step 1 must be verified *before* step 3, and that ordering is the point: the v7 shim means the firmware builds and runs correctly on the new libraries while still spelling `StaticJsonDocument<N>` at 35 of its 36 sites. That property is what makes the sequence possible, and it is temporary — it disappears the moment step 3 begins.

**Revised during implementation.** Step 1 cannot be source-free. `createShow` parses into `StaticJsonDocument<JSON_DOC_LARGE>` and calls a `std::function` declared over `StaticJsonDocument<JSON_DOC_MEDIUM>`; v6 bridged that with a converting constructor, v7 does not, and the build fails at `ShowFactory.cpp:230`. The `const JsonDocument&` retyping of `ShowConstructor` and `parseColors` — originally step 3's last task — is therefore pulled into step 1, because nothing compiles until it lands.

The cost is that step 1's hardware smoke test now covers a dependency bump *and* a signature change, so a failure there has two candidate causes. This was accepted over the alternative (retyping `ShowFactory.cpp:219` to `JSON_DOC_MEDIUM` as a one-token stopgap) because the stopgap is discarded by step 3 anyway, and because the two changes fail in visibly different ways: a library regression shows up as a route or protocol fault, whereas a signature regression shows up as wrong show parameters.

`-Werror=deprecated-declarations` is added at the start of step 3, not step 1, for exactly this reason. Added earlier it would break step 1's build for reasons unrelated to step 1.

## Decision 7 — Delete `JSON_DOC_*` rather than repurpose it as a content-length cap

Six constants in `Config.h:19-24` currently sized six categories of document. Under v7 they size nothing. There is a superficially attractive second life for them as `setMaxContentLength()` values, since `AsyncCallbackJsonWebHandler` defaults to 16384 bytes and that is a `malloc` driven by an attacker-supplied `Content-Length`.

Rejected, because the two numbers measure different things. `JSON_DOC_MEDIUM` was a guess at an *in-memory parsed* size — nodes, string copies, alignment. `_maxContentLength` bounds the *wire body*. Mapping one onto the other would silently reject valid requests wherever the old guess was tight, which is precisely the class of bug the v6 comment at `WebServerManager.cpp:165` records.

The DoS surface is real but pre-existing and unchanged: today's handlers also allocate from `Content-Length`-driven segments on a device with no PSRAM, and the server is on a LAN behind whatever the user's network provides. If a cap is wanted it should be one deliberate number chosen against the largest legitimate body — the ColorRanges `/api/show` payload — and set uniformly. That is a separate decision from deleting six constants that no longer do anything, and it is out of scope here.

## Decision 8 — Install the middleware; it covers the new handlers too

`AccessLogger` exists, compiles, and is never installed (`WebServerManager.cpp:1468`, commented out). Its `run()` is the only caller of `markServedRequest()`, so `hasServedAnyRequest()` is permanently `false` and `Network.cpp:377`'s OTA auto-confirm gate cannot be satisfied.

This is in scope because the fix has to be verified against the new routing path, not the old one. The middleware chain wraps the handler generically:

```cpp
// WebRequest.cpp:159
_server->_runChain(this, [this]() {
  return _handler ? _handler->_runChain(this, [this]() { _handler->handleRequest(this); })
                  : send(501);
});
```

`_handler` is whatever the router selected, so a handler registered via `addHandler(new AsyncCallbackJsonWebHandler(...))` is wrapped identically to one registered via `server.on(...)`. Uncommenting the line is correct both before and after step 4.

One caveat inherited from the existing code, worth noting while touching it: `AccessLogger::run` stores `request->client()->remoteIP().toString().c_str()` and `request->url().c_str()` into `const char*` locals. Both are `String` temporaries destroyed at the end of the full expression, so the pointers dangle before `snprintf` reads them. This is a pre-existing defect in code that has never executed, and it will begin executing the moment the middleware is installed.

## Decision 9 — Give `ShowFactory` a host-side test before changing its signature

`ShowFactory` is excluded from the native build (`platformio.ini:14`), and `grep -rn Json test/` finds nothing. The `#else` branch at `ShowFactory.cpp:233` instantiates `StaticJsonDocument` while `ShowFactory.h` includes `<ArduinoJson.h>` only under `#ifdef ARDUINO` — that branch cannot compile, which is how it went unnoticed that no native test ever reaches a JSON document.

`platformio.ini` already carries the intent, commented out at line 24:

```ini
;lib_deps =
;	bblanchon/ArduinoJson@^6.21.3
```

Uncommenting it as `^7.4.3` and adding `+<ShowFactory.cpp>` to `build_src_filter` makes the 12 show constructors and `parseColors` host-testable. That matters specifically because step 3 changes `parseColors`'s parameter type and the `JsonArrayConst` note at `ShowFactory.cpp:32` is a v6 artifact whose v7 behaviour nothing currently checks.

The alternative — dropping the `#ifdef ARDUINO` guards so the header is unconditionally ArduinoJson-dependent — is the same work with less ceremony, since the native env gains the dependency either way. Preferred, and it deletes the uncompilable `#else` branch rather than repairing it.

The routing and body-framing behaviour of step 4 remains untestable on the host; ESPAsyncWebServer 3.11.0 added Arduino-Emulator support for exactly this, but adopting it is a change of its own.
