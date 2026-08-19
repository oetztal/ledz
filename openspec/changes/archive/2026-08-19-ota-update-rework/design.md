## Context

The current `OTAUpdater` predates this proposal. It was written when the device was single-purpose and "OTA works at all" was the goal. Today, the project has matured:

- **Dual-core S3.** Network task pinned to Core 0 with AsyncTCP and WiFi; LED show on Core 1 at 100 Hz. The old OTA makes the network task wait minutes, which means touch polling, NTP, mDNS, and the entire web UI freeze.
- **Embedded HTML.** The web UI ships inside the firmware, so API changes are atomic — old clients cannot talk to a new server or vice versa across devices. This removes a whole class of API versioning concerns.
- **ShowController pattern.** Long-running operations cross the Core 0 / Core 1 boundary via a FreeRTOS queue (`ShowCommand`). OTA is a similar long-running operation but lives entirely on Core 0 today; the new design treats it as a peer to show changes.
- **ConfigManager::requestRestart** already exists. The worker just calls `config.requestRestart(2000)` instead of `ESP.restart()` directly, letting the Network task drain any in-flight responses.

The klimacontrol repo at `https://github.com/wuan/klimacontrol` solved a closely related problem for ESP32-S2 (single core, no PSRAM). Its `src/OTAUpdater.{h,cpp}` is the closest reference implementation. This change is not a port — the S2 single-core constraint, the absence of an `updateInProgress` heap-pressure signal, and the S2's tiny internal heap drove klimacontrol toward `xTaskCreateStatic` and a custom mbedTLS allocator. None of those apply to us; what *does* apply is the architecture: state machines, RAII HTTP client, redirected streaming download, atomic guard, separate worker task. This design adopts the architecture and discards the S2-specific workarounds.

## Goals / Non-Goals

**Goals:**

- TLS is actually verified end-to-end against Mozilla's CA bundle shipped in the IDF.
- HTTP handlers for `/api/ota/*` return in microseconds; the longest blocking call is `xTaskCreate` (~tens of microseconds).
- The web UI can observe download progress in real-time.
- The browser cannot dictate which firmware is flashed — only whether the device's own check result is applied.
- Semver-aware version comparison with an explicit downgrade escape hatch.
- Auto-confirm with sensible defaults and a manual override.
- Optional SHA-256 verification path that adds ≤200 bytes RAM.
- Preserve rollback semantics: unconfirmed images can still be rolled back by the bootloader.

**Non-Goals:**

- Fleet management (no telemetry of update success/failure, no scheduled rollout across devices).
- Signed firmware with PKI (no manufacturer-side signing key infrastructure; the CA-bundle + GitHub TLS model is the trust root).
- Delta updates (full-image downloads only).
- Firmware compression at the wire level (GitHub already provides `.bin`; we don't repack).
- An automatic background update check (config flag exists but stays at 0; users trigger manually).
- Hot updates without a reboot (ESP-IDF OTA always requires a reboot to switch partitions).
- Multi-device firmware orchestration (single-device flow only).

## Decisions

### 1. Use `esp_http_client` with `crt_bundle_attach`, drop Arduino `HTTPClient`

**Decision:** Replace `WiFiClientSecure + HTTPClient` with `esp_http_client` configured to attach the IDF CA bundle.

```cpp
extern "C" esp_err_t esp_crt_bundle_attach(void *conf);

esp_http_client_config_t config{};
config.url = downloadUrl.c_str();
config.timeout_ms = TIMEOUT_MS;
config.crt_bundle_attach = esp_crt_bundle_attach;  // ← verifies GitHub's cert
config.buffer_size = 8192;       // GitHub's ~5 KB CSP header block
config.buffer_size_tx = 2048;    // GitHub's ~860-byte signed CDN redirect URL
```

**Why:** The current `HTTPClient + WiFiClientSecure` pair, even with `setCACertBundle()`, has known issues with the way Arduino-ESP32 wraps the underlying IDF client (klimacontrol found the same). `esp_http_client` directly is the supported path, and `crt_bundle_attach` is the only way to actually verify against the embedded Mozilla bundle. `setInsecure()` is removed entirely.

**Alternatives considered:**

- *Stick with Arduino HTTPClient + bundle.* Tested and works in many projects, but the `setCACert(NULL) + setInsecure()` combination in the current code is a real footgun — the comment is wrong and any future contributor could reintroduce the same bug. Direct IDF usage removes the wrapper that hides the verification mode.
- *Pin GitHub's cert fingerprint.* Smaller flash cost (~2 KB vs ~30 KB) but breaks on every cert rotation (~90 days) and requires OTA-of-the-OTA-handler. The CA bundle pays for itself.

### 2. RAII `HttpClient` wrapper with redirect following

**Decision:** A small `HttpClient` struct owns the `esp_http_client_handle_t` and auto-cleans up on scope exit:

```cpp
struct HttpClient {
    esp_http_client_handle_t handle = nullptr;
    explicit HttpClient(const esp_http_client_config_t& config) : handle(esp_http_client_init(&config)) {}
    ~HttpClient() { if (handle) { esp_http_client_close(handle); esp_http_client_cleanup(handle); } }
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    explicit operator bool() const { return handle != nullptr; }

    int openWithRedirects(int maxRedirects = 5) { /* loop 301/302/307/308 */ }
};
```

**Why:** Without RAII, every error path must remember to call `esp_http_client_close + esp_http_client_cleanup`. Today the code has a 30-line block of `http.end()` calls that are easy to miss on a new error path. RAII makes "did we clean up?" a static question with one answer. Copy is deleted so there's no risk of double-cleanup.

**Alternatives considered:**

- *Hand-roll a `goto cleanup` pattern in each function.* Idiomatic C, hostile to refactoring, easy to skip on a new error path.
- *Use `std::unique_ptr` with a custom deleter.* Same effect as RAII but adds an indirection and a heap allocation that wasn't there before.

### 3. `CheckState` and `UpdateState` are independent state machines

**Decision:** Two enum state machines, not one. The check can finish (Done or Failed) without an update being started; the update can be in flight while no check is happening.

```
CheckState:   Idle ──startCheck──► InProgress ──ok──► Done
                                              └──fail─► Failed

UpdateState:  Idle ──startUpdate──► Downloading ──complete──► Flashing ──ok──► Pending
                                                                     └──fail─► Failed
```

**Why:** They are conceptually separate: the check runs once per "Check for Updates" click; the update runs once per "Install" click. A user can check for updates multiple times without installing. A check failure should not block a fresh check, and an update failure should not block a re-check.

**Alternatives considered:**

- *Single combined `OtaState` enum with 12 values.* Tempting for a unified mental model, but the state combinations (e.g., `CheckDoneUpdateIdle` vs `CheckIdleUpdateDownloading`) become a Cartesian explosion. Two separate state machines are easier to reason about and to serialize into the JSON status response.
- *Reuse the existing `ShowCommand` FreeRTOS queue.* Architecturally tempting, but OTA is fundamentally a single long-running operation, not a stream of small commands. The queue's 5-deep bounded buffer would invite complexity (how do you queue a check? what happens if it's full?). A worker task is simpler.

### 4. Atomic `updateInProgress` set during both check and update

**Decision:** A single `std::atomic<bool> updateInProgress` is set to `true` at the start of `checkForUpdate()` *and* at the start of `performUpdate()`. Set via `compare_exchange_strong` on the update path to close the check-then-set TOCTOU window.

```cpp
bool expected = false;
if (!updateInProgress.compare_exchange_strong(expected, true)) {
    ESP_LOGW(TAG, "Update already in progress");
    return false;
}
```

**Why:** The check holds a TLS connection to `api.github.com` plus a `JsonDocument` for the duration. Together these can drop free heap by tens of KB. If a network task low-heap watchdog exists or is added later, it could trip during the check. Setting the flag here is a stable interface point: anything that needs to know "the heap is under stress" reads the same flag.

**Alternatives considered:**

- *Two separate flags (`checkInProgress`, `updateInProgress`).* Cleaner semantics, but no current code reads either flag, and the cost of consolidating later is low (the union of "either is busy" is what we care about).
- *Skip during check.* Cheaper, but the first time a low-heap watchdog is added we'll get a hard-to-reproduce bug.

### 5. Worker task pinned to Core 1, LED show suspended during flash

**Decision:** The OTA worker task is created via `xTaskCreatePinnedToCore(..., /* core */ 1, ...)`. Before starting the download, the worker suspends `showTaskHandle`; after success or abort, it resumes the task (or leaves it suspended if the device is about to restart).

```cpp
TaskHandle_t showHandle = xTaskGetHandle("LedShow");
if (showHandle) vTaskSuspend(showHandle);
// ... performUpdate ...
if (success) config->requestRestart(2000);
if (showHandle) vTaskResume(showHandle);
vTaskDelete(nullptr);
```

**Why:** Core 0 is occupied by AsyncTCP + WiFi + mbedTLS. Running OTA there means competing with the AsyncTCP event loop for the same core during a multi-minute TLS stream — exactly what we're trying to escape. Core 1 is LED show at 100 Hz; suspending it during flash yields:
- Visual signal: LEDs going dark = "updating"
- ~10 KB stack reclaimed from the LED show context
- A core entirely dedicated to the OTA work

When the new firmware boots, the LEDs come back on. That's the confirmation.

**Alternatives considered:**

- *Don't pin, let FreeRTOS schedule.* Works on paper; in practice the WiFi/TLS heap fragmentation on S2 made klimacontrol reach for `xTaskCreateStatic`. S3 has more headroom, but pinning is free insurance.
- *Pin to Core 0.* Wrong direction — that core is the one we're trying to unblock.
- *Keep LEDs on during flash.* Loses the visual signal and the small heap savings. The two-second "LEDs dark" interruption is acceptable UX.

### 6. Dynamic 8-12 KB stack via `xTaskCreate`, not `xTaskCreateStatic`

**Decision:** Use `xTaskCreatePinnedToCore` with `usStackDepth = 10240` (10 KB) for the OTA worker. Don't reserve a BSS-resident stack.

**Why:** klimacontrol uses `xTaskCreateStatic` with BSS-reserved stacks (12 KB update, 7 KB check) because the ESP32-S2's internal SRAM is so small that runtime allocation fails after WiFi/TLS have fragmented the heap. The S3 has roughly 4× the internal SRAM; we measured ~150-200 KB free heap during OTA. Dynamic allocation works fine. The high-water-mark log at worker exit will reveal the real minimum so we can tune later.

**Alternatives considered:**

- *BSS static stack.* Carries over from klimacontrol but solves a problem we don't have. Costs ~10 KB of permanent BSS; not worth it.
- *Smaller stack (4 KB).* Almost certainly stack-overflows — klimacontrol measured 9.3 KB HWM on S2 just for TLS working set.

### 7. Hand-rolled semver comparator, no new dependency

**Decision:** ~80 lines in an anonymous namespace at the top of `OTAUpdater.cpp`. Parses `MAJOR.MINOR.PATCH[-PRERELEASE][+BUILD]`, compares per the semver spec.

```cpp
namespace {
struct SemVer { int major, minor, patch; String prerelease; };
std::optional<SemVer> parseSemVer(const String& tag);
int compareSemVer(const SemVer& a, const SemVer& b); // -1, 0, 1
}
```

**Why:** Pulling in a semver library costs ~5-10 KB flash and a dependency we'd own. The comparator only needs to handle the formats we actually tag with (X.Y.Z, X.Y.Z-rc.N), and the rules are documented in RFC-2119-style bullets that fit in one screen of C++. Hand-rolling is justified.

**Fallback policy for unparseable tags:**

- Both `FIRMWARE_VERSION` and `info.version` unparseable → treat as `>` (current version, do not update).
- `FIRMWARE_VERSION` parseable, `info.version` not → refuse update, return `unparseable_tag` error.
- `FIRMWARE_VERSION` not parseable, `info.version` parseable → treat as `<` (update available).

This avoids the "is `latest` newer than `v1.2.4`?" surprise while still tolerating placeholder tags like `v0.0.0-dev`.

**Alternatives considered:**

- *Use ArduinoJson's string compare after normalizing.* Doesn't actually solve the problem — `v1.2.4-rc.1` vs `v1.2.4` is still ambiguous.
- *Add `semver` library.* 5-10 KB flash cost. Not worth it.

### 8. `force=true` is a query parameter, not a body field

**Decision:** `POST /api/ota/update?force=true` — no body. `force=false` (or omitted) is the default.

**Why:** The endpoint takes no body otherwise. Query parameters are simpler to parse (zero JSON deserialization in the handler) and keep the API idempotent. The UI sends the flag based on a checkbox state.

**Alternatives considered:**

- *Body parameter `{force: true}`.* More REST-ful, but requires JSON parsing for a single bool. Unnecessary work.
- *Separate `/api/ota/downgrade` endpoint.* Splits the API surface for no benefit — same code path, same worker, just a different URL.

### 9. Auto-confirm: first HTTP request served AND ≥5 minutes uptime

**Decision:** In the Network task's 1Hz tick:

```cpp
if (OTAUpdater::hasUnconfirmedUpdate()) {
    bool minUptimeMet = (millis() - bootTimeMs) >= OTA_AUTO_CONFIRM_MIN_UPTIME_MS;
    bool requestServed = webServer->hasServedAnyRequest();
    if (minUptimeMet && (!OTA_AUTO_CONFIRM_REQUIRE_REQUEST || requestServed)) {
        OTAUpdater::confirmBoot();
        ESP_LOGI(TAG, "Auto-confirmed after %lu ms uptime", millis() - bootTimeMs);
    }
}
```

`bootTimeMs` is captured in `setup()`. `webServer->hasServedAnyRequest()` reads a `std::atomic<bool>` set inside any WebServerManager handler.

**Why:** "5 minutes" catches the common failure modes (boot crash, init crash, immediate watchdog trip) without making the user wait half a day. "First HTTP request served" is the strongest "the firmware actually works" signal — it proves AsyncTCP, request routing, JSON serialization, and FreeRTOS scheduling are all alive. Both conditions together is robust without being annoying.

**Manual `/api/ota/confirm` stays** for users who want immediate confirmation or who run headless with `OTA_AUTO_CONFIRM_REQUIRE_REQUEST = false`.

**Alternatives considered:**

- *Pure time (e.g. 4 hours).* Simpler, but a firmware that never serves HTTP stays unconfirmed forever. User wonders "did it work?"
- *Event only (first HTTP request, no time floor).* A 10-second flash that immediately resets would auto-confirm via a stray HTTP keep-alive. Time floor prevents this.

### 10. SHA-256 verification is opt-in via `OTA_ENABLE_SHA256_VERIFICATION`, default OFF

**Decision:** New `#define OTA_ENABLE_SHA256_VERIFICATION 0` in `OTAConfig.h`. When 1, the worker:
1. Computes `<download_url>.sha256`, fetches it (~100 bytes).
2. Parses hex into a 32-byte expected hash.
3. Streams the `.bin`, computing the running SHA-256 in parallel via `mbedtls_sha256_update`.
4. Compares `actual` to `expected` before `Update.end()`. On mismatch, `Update.abort()`.

**Why:** Technically feasible with ~184 bytes of stack (mbedTLS is already linked for TLS), but the value-to-complexity ratio for ledz's threat model (personal device, GitHub+TLS) doesn't favor enabling by default. A release-process change is required (every release must publish `.bin.sha256`); making it optional lets that burden be opt-in.

**When the flag is 1 and the `.sha256` file 404s, the update fails** (strict mode). Silent fallback would defeat the purpose — if you enable SHA, missing hash = no update.

**Why the design includes it anyway:** ~50 LOC, ~2 KB flash, zero heap. Having the path wired but disabled means a future change to enable it by default is a one-line `#define` flip plus a release-script addition, not a refactor.

**Alternatives considered:**

- *Always on.* Higher security ceiling, but couples every release to a `.sha256` file. For a personal project, that's an annoying constraint.
- *Buffer the whole `.bin` to RAM, hash, then re-flash.* ~988 KB won't fit in our ~150 KB free heap. Not viable.

### 11. `FirmwareInfo` drops `releaseNotes`; UI parses from new `/api/ota/check` body field

**Decision:** The struct no longer holds a full `releaseNotes` string. The check endpoint fetches the field but caps it at 2 KB by truncating the JSON body to `JSON_DOC_LARGE` (currently 4096; bump to 8192 if needed). The settings page receives the truncated text and renders it as plain-text.

**Why:** GitHub release notes can be hundreds of KB. Holding the whole thing in the struct wastes heap between the check and the user clicking "Install". A 2 KB cap is plenty for a human-readable summary; releases longer than that should link to GitHub.

**Alternatives considered:**

- *Include the full release notes.* Wasteful; today the field is parsed and largely unused.
- *Skip parsing entirely.* The user wants to see the changelog, so we have to parse it.

### 12. Markdown rendering is escape + linkify, not a library

**Decision:** In `settings.html`, `releaseNotes` is rendered as:

```js
function renderReleaseNotes(markdown) {
  const escaped = markdown.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
  const linked = escaped.replace(/(https?:\/\/[^\s<]+)/g, '<a href="$1" target="_blank" rel="noopener">$1</a>');
  return linked.replace(/\n/g, '<br>');
}
```

**Why:** GitHub release notes use Markdown. Full Markdown rendering needs a library (~3-5 KB compressed). Most release notes are short bullet lists and links — escape + URL-linkify + newlines captures 90% of the value at zero flash cost.

**Alternatives considered:**

- *Inline Markdown renderer (e.g., `marked` lite).* Adds 3-5 KB; for a personal device, not worth it.
- *Show as `<pre>` raw.* Easy, but URLs aren't clickable. Today's behavior.

### 13. `requestRestart(2000)` instead of `requestRestart(1000)`

**Decision:** The worker schedules `config.requestRestart(2000)` (was 1000) after a successful flash. Two seconds is enough for AsyncTCP to drain the HTTP response to the `/api/ota/update` POST before `ESP.restart()` runs.

**Why:** The HTTP handler has already responded 202 to the POST before the worker starts. But if the UI is polling `/api/ota/status` on a `setInterval(1000)`, the next poll might land mid-flash. 2 seconds gives one clean extra poll cycle before the device goes down. Marginal improvement; cheap.

**Alternatives considered:**

- *Direct `ESP.restart()` in the worker.* Loses the chance for AsyncTCP to drain. Today's behavior.
- *`requestRestart(5000)`.* Overcautious — adds 3 seconds of "is it restarting?" UX with no real benefit.

## Risks / Trade-offs

- **esp_crt_bundle flash cost (~30 KB).** Current firmware is at 988 KB of 1856 KB partition. Adding the bundle leaves ~838 KB headroom. Mitigation: none needed; the budget accommodates it. If it doesn't, `ld` will fail with a clear error.
- **Worker task stack overflow.** 10 KB is a guess. Mitigation: log `uxTaskGetStackHighWaterMark` at task exit. If HWM approaches 0, bump the size in a follow-up.
- **Heap fragmentation from TLS.** The check task opens a TLS connection; the update task does too. Fragmentation between them could cause the second task's `xTaskCreate` to fail. Mitigation: monitor free heap; if it falls below 60 KB, log a warning and refuse to start the worker.
- **Boot-time regression.** The `OTA_AUTO_CONFIRM_MIN_UPTIME_MS = 300000` default means a buggy firmware can roll back for up to 5 minutes after boot — visible as the device rebooting twice in quick succession. Mitigation: clearly documented; the Network task logs the uptime at confirmation time so it's visible in serial logs.
- **`force=true` UI misuse.** A confused user might enable it and install a known-bad older version. Mitigation: the checkbox is labeled clearly, appears only when relevant, and the server-side log includes "forced downgrade" so it's visible.
- **Changelog XSS.** GitHub allows raw HTML in release notes body. Mitigation: HTML escape + URL linkify. No raw HTML passes through. Tested by sending `<script>alert(1)</script>` in a test release — must render as text.
- **Semver comparator bugs.** Hand-rolled is more likely to have edge cases than a battle-tested library. Mitigation: comprehensive native tests (`test/test_ota/test_semver.cpp`) covering: equal versions, prerelease ordering (`1.0.0-rc.1 < 1.0.0`), missing components, leading zeros, unparseable inputs.
- **`updateInProgress` false-negatives on abort.** If `performUpdate()` is called and panics, the flag stays set. Mitigation: RAII guard in `performUpdate()` clears it on every return path; worker task self-deletes via `vTaskDelete(nullptr)` which is async-safe.
- **Existing devices with `/api/ota/update` body parsing.** Old firmware receiving a new settings page wouldn't happen (atomic deploy). New firmware receiving an old settings page is impossible (page is embedded). No compatibility shim needed.
- **klimacontrol reference is moving.** The reference repo could change. Mitigation: this design is captured here, not by linking to a moving target. If klimacontrol diverges, this change is unaffected.

## Migration Plan

No data migration. Existing devices:
1. Update to the new firmware (via the *old* OTA mechanism, which still works — the bug is "TLS not verified", not "doesn't work at all").
2. After the new firmware boots, the new `/api/ota/*` endpoints take effect.
3. Stored `FIRMWARE_VERSION` continues to work; the semver parser accepts any string and falls back gracefully.
5. Partition table unchanged. `nvs` unchanged. `otadata` is rewritten by the ESP-IDF bootloader as usual.

The new firmware is a full image (no delta). It will live in the *other* OTA partition and boot there.

## Open Questions

None. All design decisions captured above.