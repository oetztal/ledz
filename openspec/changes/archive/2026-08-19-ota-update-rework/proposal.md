## Why

The current OTA implementation (`src/OTAUpdater.{h,cpp}`) has four problems that combine to make firmware updates both insecure and unreliable:

1. **TLS is not actually verified.** `createSecureClient()` calls `setCACert(NULL)` (commented "use built-in bundle") then `setInsecure()` (commented "skip verification"). `setInsecure()` overrides the bundle — every GitHub release download is MITM-vulnerable today, despite the documentation claiming otherwise.
2. **The HTTP handler blocks for minutes.** `OTAUpdater::performUpdate()` runs synchronously inside the body-upload handler of `POST /api/ota/update`. While the firmware is streaming, AsyncTCP cannot serve any other request — `/api/status`, brightness changes, show changes, the settings page itself all stall for 2-5 minutes. The LED show on Core 1 also keeps rendering during this window with no visual signal that an update is in progress.
3. **The download URL is trusted from the client.** The browser POSTs `{download_url, size}` and the device flashes whatever it receives. A malicious page on the same network, or any compromised browser, can push an arbitrary `.bin` to the device.
4. **No semver awareness, no downgrade path, no auto-confirm.** Version comparison is `info.version != FIRMWARE_VERSION` (string equality) — `v1.2.3-rc.1` and `v1.2.3` are "different". There is no way to intentionally downgrade after a bad release. Boot confirmation requires a manual `/api/ota/confirm` POST that real users will never run, leaving every device with an unconfirmed image subject to automatic rollback forever.

The klimacontrol repo (`https://github.com/wuan/klimacontrol`) solved an adjacent version of this problem for ESP32-S2. This change adapts those lessons to ledz's ESP32-S3 + dual-core + no-PSRAM reality.

## What Changes

- **TLS is actually verified** against the IDF CA bundle (`esp_crt_bundle_attach`) via `esp_http_client` instead of `WiFiClientSecure + HTTPClient`. `setInsecure()` is removed.
- **OTA work moves off AsyncTCP.** Two state machines (`CheckState`, `UpdateState`) let `/api/ota/check` and `/api/ota/update` return in microseconds; a dedicated FreeRTOS task pinned to Core 1 does the actual download + flash.
- **`updateInProgress` atomic flag** is set during both check and update (not just update). Guards against concurrent OTA workers and signals to other tasks that the heap is under stress.
- **LED show suspended during flash** for visual signal ("LEDs go dark = updating") and a small heap reduction. Restored after the worker exits or aborts.
- **RAII `HttpClient` wrapper** around `esp_http_client` with `openWithRedirects(5)` that follows 301/302/307/308 (current code only handles 301/302). Body is drained before close. JSON parsed via a streaming `Reader` adapter directly on `esp_http_client_read`, no intermediate buffer.
- **Hand-rolled semver comparator** (anonymous namespace in `OTAUpdater.cpp`, ~80 lines, no new dependency) replaces string equality. Falls back to "always newer" only for unparseable tags *if* `FIRMWARE_VERSION` is also unparseable in the same way — otherwise refuses the update.
- **`POST /api/ota/update?force=true`** allows downgrades and same-version reinstalls. Query parameter, no body. A labeled checkbox in the UI surfaces the option only when the latest version is `≤` current.
- **Auto-confirm after first HTTP request served AND ≥5 minutes uptime** (configurable via `OTAConfig.h`). Replaces the current `OTA_AUTO_CONFIRM_DELAY_MS = 0` policy. Manual `/api/ota/confirm` is kept as an escape hatch.
- **Changelog rendered in the settings page** as plain text with HTML escaping and URL linkification. No Markdown library.
- **Optional streaming SHA-256 verification** behind `OTA_ENABLE_SHA256_VERIFICATION` (default OFF for v1). When enabled, the worker downloads `firmware.bin.sha256` first, then streams `.bin` while computing the running hash, and aborts on mismatch. Memory overhead: ~184 bytes stack. Releases must publish `.sha256` for this to work.
- **All redirects fetched with `buffer_size=8192`** (GitHub's ~5 KB CSP header block) and **`buffer_size_tx=2048`** (GitHub's ~860 byte signed CDN redirect URL).

## Capabilities

### New Capabilities

- `ota-update`: Describes the firmware-over-the-air update flow — GitHub release check, secure HTTPS download, worker-driven flashing with progress observation, rollback support, semver-aware version handling, optional SHA-256 verification, and auto-confirm semantics.

### Modified Capabilities

None. There is no prior OTA spec.

## Impact

- `src/OTAUpdater.h` — `FirmwareInfo` (drop `releaseNotes`), add `Progress` struct, `CheckState`/`UpdateState` enums, `startBackgroundCheck`, `startBackgroundUpdateFromLatestCheck`, `getCheckState`, `getProgress`, `isUpdateInProgress`. Remove the synchronous `checkForUpdate` and `performUpdate` public API or relegate them to private.
- `src/OTAUpdater.cpp` — full replacement of the HTTP path. New `esp_http_client` based flow, `HttpClient` RAII wrapper, `EspHttpReader` streaming adapter, `OtaVerifyContext` (SHA-256 hook), worker task, semver comparator, `updateInProgress` atomic.
- `src/OTAConfig.h` — new flags `OTA_AUTO_CONFIRM_MIN_UPTIME_MS`, `OTA_AUTO_CONFIRM_REQUIRE_REQUEST`, `OTA_ENABLE_SHA256_VERIFICATION`, `OTA_SHA256_ASSET_SUFFIX`. Existing `OTA_AUTO_CHECK_ON_BOOT`, `OTA_AUTO_CHECK_PERIODIC`, `OTA_ENABLE_FORCE_PIN`, `OTA_AUTO_CONFIRM_DELAY_MS`, `OTA_ENABLE_LED_FEEDBACK` become no-ops or are removed (none of them were wired up).
- `src/WebServerManager.cpp` — `setupAPIRoutes()` rewires four endpoints (`/api/ota/check`, `/api/ota/status`, `/api/ota/update`, `/api/ota/confirm`). `/api/ota/status` returns the new state-machine JSON. `/api/ota/check` and `/api/ota/update` return `202 {started:true}` synchronously and spawn workers.
- `data/settings.html` — `loadOTAStatus`, `checkForUpdates`, `performUpdate`, plus a new `pollOTAStatus` loop driven by `setInterval`. Add a real progress bar (percent + bytes), a "Allow installing older or equal version" checkbox, and a `<pre id="releaseNotes">` block with linkified URLs. Replace the `alert("Update started! ...refresh in 30 seconds")` with auto-reconnect logic that polls `/api/ota/status` until `update.state == pending` and then watches for `/api/status` to respond again.
- `data/control.html` — optional: small "Update available: vX.Y.Z" badge in the top bar when `update.state != idle` or when a check has detected a newer version, linking to `/settings`.
- `scripts/release.sh` (new) — wraps `pio run` + `sha256sum` + `gh release create` so each release publishes `firmware.bin` and `firmware.bin.sha256` together.
- `scripts/compress_web.py` — no change (existing pipeline already handles settings.html).
- `docs/OTA_FIRMWARE_UPDATES.md`, `docs/OTA_QUICK_START.md`, `docs/OTA_IMPLEMENTATION_SUMMARY.md`, `docs/OTA_INDEX.md` — rewritten or deleted. `OTA_REFERENCE_CARD.md` and `examples/OTA_INTEGRATION_EXAMPLE.cpp` — deleted (predate the design and would mislead).
- `test/test_ota/` (new) — native-environment tests for: (a) semver comparator (parsing, ordering, prerelease handling, edge cases), (b) state machine transitions (Idle → InProgress → Done / Failed, Idle → Downloading → Flashing → Pending / Failed), (c) JSON `body` truncation when release notes exceed buffer, (d) `updateInProgress` CAS correctness.
- `platformio.ini` — register `test/test_ota` under `[env:native]` test configs.

**Backward compatibility:** existing devices on the old OTA endpoints will see the new responses. `/api/ota/status` JSON gains fields (additive), so old settings pages still work until rebuilt. `/api/ota/update` no longer reads a body — the old `{download_url, size}` body is ignored, the update URL is taken from the device's own check. Since the embedded HTML ships with the firmware, there is no version skew risk in practice.

**No partition table change.** Existing 4MB layout (`app0`/`app1` × 1856 KB) accommodates both old and new images. The `esp_crt_bundle` adds ~30 KB flash; current 988 KB usage leaves ~830 KB headroom.