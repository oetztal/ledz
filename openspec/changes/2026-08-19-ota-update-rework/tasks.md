## 1. Establish esp_http_client + RAII infrastructure

- [ ] 1.1 Add `#include <esp_http_client.h>` and `#include <mbedtls/sha256.h>` (the latter is gated by `OTA_ENABLE_SHA256_VERIFICATION`) to `src/OTAUpdater.cpp`
- [ ] 1.2 Declare `extern "C" esp_err_t esp_crt_bundle_attach(void *conf);` near the top of `src/OTAUpdater.cpp` (matches klimacontrol's pattern)
- [ ] 1.3 Implement `HttpClient` RAII wrapper struct (~40 lines): `esp_http_client_handle_t handle`, deleted copy, `openWithRedirects(int maxRedirects = 5)` returning status or -1, `operator bool()` for init check, destructor calling `close + cleanup`
- [ ] 1.4 Implement `EspHttpReader` adapter (~15 lines): holds `esp_http_client_handle_t`, exposes `int read()` and `size_t readBytes(char*, size_t)` compatible with ArduinoJson's `Reader` concept

## 2. Define new public API in OTAUpdater.h

- [ ] 2.1 Add `enum class CheckState : uint8_t { Idle, InProgress, Done, Failed }` and `enum class UpdateState : uint8_t { Idle, Downloading, Flashing, Pending, Failed }`
- [ ] 2.2 Add `struct Progress { UpdateState state; uint8_t percent; size_t bytes_written; size_t expected_bytes; unsigned long started_at_ms; String error_message; }`
- [ ] 2.3 Modify `struct FirmwareInfo`: keep `version`, `name`, `downloadUrl`, `size`, `isValid`; drop `releaseNotes` (move to a new `Changelog` field capped at 2 KB on parse); add `String changelog`
- [ ] 2.4 Add static methods: `static bool startBackgroundCheck(const char* owner, const char* repo);`, `static bool startBackgroundUpdateFromLatestCheck();`, `static CheckState getCheckState();`, `static const FirmwareInfo& getCheckResult();`, `static Progress getProgress();`, `static bool isUpdateInProgress();`
- [ ] 2.5 Keep existing static methods: `confirmBoot`, `hasUnconfirmedUpdate`, `getRunningPartitionInfo`, `getMemoryInfo`, `hasEnoughMemory`
- [ ] 2.6 Remove the synchronous `static bool checkForUpdate(...)` and `static bool performUpdate(...)` from the public API (relocate as `private:` if needed for the worker task)

## 3. Implement state machine and atomic guard in OTAUpdater.cpp

- [ ] 3.1 Add `static inline std::atomic<bool> updateInProgress{false};` and `static inline CheckState checkState = CheckState::Idle;` and `static inline FirmwareInfo checkResult{};` and `static inline Progress progress{};`
- [ ] 3.2 Add `static inline SemaphoreHandle_t checkResultMutex() { static SemaphoreHandle_t m = xSemaphoreCreateMutex(); return m; }` lazy-init pattern
- [ ] 3.3 Implement `getCheckState()` and `getCheckResult()` as mutex-guarded snapshots
- [ ] 3.4 Implement `getProgress()` returning a copy of the static `progress` struct
- [ ] 3.5 Implement `publishProgress(int percent, size_t bytes)` for the worker to call from inside the download loop

## 4. Implement `checkForUpdate()` (private, with new semantics)

- [ ] 4.1 Set `updateInProgress = true` at entry, clear via RAII `InProgressGuard` on every return path
- [ ] 4.2 Set `checkState = InProgress` and `checkResult = FirmwareInfo{}` before opening the HTTP connection
- [ ] 4.3 Build the API URL, configure `esp_http_client` with `crt_bundle_attach`, `timeout_ms = TIMEOUT_MS`, `buffer_size = 8192`, `buffer_size_tx = 2048`
- [ ] 4.4 Open via `HttpClient::openWithRedirects()`. On any non-200 status, set `checkState = Failed`, populate `error_message`, return false
- [ ] 4.5 Build the `JsonDocument` filter: `tag_name`, `name`, `assets`, `body` (cap at ~2 KB). Allocate `DynamicJsonDocument(Config::JSON_DOC_OTA)` for parsing
- [ ] 4.6 Stream-parse via `EspHttpReader`. On `DeserializationError`, set `checkState = Failed` with error string, return false
- [ ] 4.7 Extract `version`, `name`, walk `assets` for the first `.bin` to populate `downloadUrl` and `size`. Cap `content` extraction at 2048 bytes (truncate with `+ … (N more bytes)` if longer)
- [ ] 4.8 Drain remaining body before `esp_http_client_close()` to release TLS buffers early
- [ ] 4.9 Call `doc.clear(); doc.shrinkToFit();` to free JSON memory before returning
- [ ] 4.10 On success: set `checkState = Done`, populate `checkResult`, return true

## 5. Implement semver comparator (anonymous namespace in OTAUpdater.cpp)

- [ ] 5.1 Add `struct SemVer { int major, minor, patch; String prerelease; };` in anonymous namespace
- [ ] 5.2 Implement `std::optional<SemVer> parseSemVer(const String& tag)` that strips optional `v` prefix, splits on `.` and `-`, returns nullopt on malformed input
- [ ] 5.3 Implement `int compareSemVer(const SemVer& a, const SemVer& b)` returning -1/0/1 per the semver spec (numeric prerelease identifiers compare as integers; alphanumeric compare lexically; numeric < alphanumeric; no prerelease > any prerelease)
- [ ] 5.4 Add `bool isNewerVersion(const String& latest, const String& current)` that handles the fallback policy described in design decision 7 (both unparseable → not newer; mixed → refuse)
- [ ] 5.5 Write `test/test_ota/test_semver.cpp` covering equal/major-difference/prerelease/unparseable/leading-zeros/missing-components cases

## 6. Implement `performUpdate()` (private, called by worker)

- [ ] 6.1 Take `const String& downloadUrl`, `size_t expectedSize`, `const std::function<void(int, size_t)>& onProgress`
- [ ] 6.2 Refuse if `expectedSize == 0`, log error, return false
- [ ] 6.3 Open TLS via `HttpClient::openWithRedirects()`. Configure the same buffer sizes as the check path
- [ ] 6.4 Validate `esp_http_client_get_content_length()` matches `expectedSize`. Mismatch → log + return false
- [ ] 6.5 `Update.begin(expectedSize, U_FLASH)`. On failure, log + return false
- [ ] 6.6 Stream loop: read up to 4096 bytes per chunk, call `Update.write(buf, n)`, call `onProgress(percent, totalRead)`, `vTaskDelay(1)`, `esp_task_wdt_reset()`
- [ ] 6.7 On any error: `Update.abort()`, log, return false
- [ ] 6.8 On loop completion: `Update.end(false)` (no reboot from this call). On failure, log + return false
- [ ] 6.9 Drain body, close HTTP client, return true
- [ ] 6.10 Memory check before starting: refuse if `ESP.getFreeHeap() < 65536`

## 7. Implement OTA worker task

- [ ] 7.1 Add `struct OtaJob { String url; size_t expected_size; bool force; }` in anonymous namespace
- [ ] 7.2 Implement `static void otaWorkerTask(void* arg)`:
  - Cast `arg` to `OtaJob*`
  - Capture `TaskHandle_t showHandle = xTaskGetHandle("LedShow")` and `vTaskSuspend(showHandle)` if non-null
  - Initialize `progress { state = Downloading, expected_bytes = job->expected_size, started_at_ms = millis() }`
  - Call `performUpdate(job->url, job->expected_size, publishProgress)` with the progress publisher
  - On success: set `progress.state = Pending`, call `Config::ConfigManager::requestRestart(2000)`
  - On failure: set `progress.state = Failed`, populate `progress.error_message`
  - If `showHandle` non-null: `vTaskResume(showHandle)`
  - `delete job`
  - Log `uxTaskGetStackHighWaterMark(nullptr)` as `ota_update HWM`
  - Set `updateInProgress = false`
  - `vTaskDelete(nullptr)`
- [ ] 7.3 Implement `static void otaCheckTask(void* arg)` similarly:
  - Cast `arg` to a small struct holding `owner`, `repo` strings
  - Call `checkForUpdate(job->owner, job->repo, info)` (private)
  - Publish to `checkState` and `checkResult` under mutex
  - Log HWM, `vTaskDelete(nullptr)`
- [ ] 7.4 Implement `startBackgroundCheck()`: take mutex, refuse if `checkState == InProgress` or `updateInProgress`, set `checkState = InProgress`, release mutex, `xTaskCreatePinnedToCore(otaCheckTask, "ota_check", 7168, job, 1, nullptr, 1)`
- [ ] 7.5 Implement `startBackgroundUpdateFromLatestCheck()`:
  - Snapshot `checkState` and `checkResult`
  - Refuse if not `Done`, or `!info.isValid`, or `info.size == 0`, or `info.downloadUrl.isEmpty()`
  - Refuse unless `info.downloadUrl.startsWith("https://github.com/")`
  - Atomically claim `updateInProgress` via `compare_exchange_strong`
  - `xTaskCreatePinnedToCore(otaWorkerTask, "ota_update", 10240, job, 1, nullptr, 1)`

## 8. Wire SHA-256 verification behind the config flag

- [ ] 8.1 Add `#define OTA_ENABLE_SHA256_VERIFICATION 0` and `#define OTA_SHA256_ASSET_SUFFIX ".sha256"` to `src/OTAConfig.h`
- [ ] 8.2 In `otaWorkerTask`, after spawning the download setup but before `performUpdate()`:
  - If the flag is 0: skip entirely
  - If the flag is 1: derive `<url>.sha256`, fetch it via a small `HttpClient`, parse the hex into a 32-byte `expected` array. On 404 or malformed hex, set `progress.state = Failed` and abort (strict mode)
- [ ] 8.3 Inside the download loop, after each chunk read and before `Update.write()`, call `mbedtls_sha256_update(&ctx, buf, n)` when the flag is enabled
- [ ] 8.4 After the loop completes, call `mbedtls_sha256_finish(&ctx, actual)` and `memcmp(actual, expected, 32)`. Mismatch → `Update.abort()`, set `progress.state = Failed`, log
- [ ] 8.5 Call `mbedtls_sha256_free(&ctx)` on all return paths

## 9. Update OTAConfig.h

- [ ] 9.1 Remove dead flags: `OTA_AUTO_CHECK_ON_BOOT`, `OTA_AUTO_CHECK_PERIODIC`, `OTA_CHECK_INTERVAL_HOURS`, `OTA_ENABLE_FORCE_PIN`, `OTA_FORCE_PIN`, `OTA_AUTO_CONFIRM_DELAY_MS`, `OTA_BOOT_VERIFY_TIMEOUT_MS`, `OTA_HTTP_TIMEOUT_MS`, `OTA_DOWNLOAD_CHUNK_SIZE`, `OTA_USE_HTTP10`, `OTA_ENABLE_LED_FEEDBACK`, `OTA_LED_PIN`, `OTA_LED_FEEDBACK_MODE`, `OTA_LOG_HTTP_HEADERS`
- [ ] 9.2 Add: `OTA_AUTO_CONFIRM_MIN_UPTIME_MS 300000`, `OTA_AUTO_CONFIRM_REQUIRE_REQUEST true`, `OTA_ENABLE_SHA256_VERIFICATION 0`, `OTA_SHA256_ASSET_SUFFIX ".sha256"`, `OTA_CHECK_TASK_STACK 7168`, `OTA_UPDATE_TASK_STACK 10240`
- [ ] 9.3 Keep `OTA_GITHUB_OWNER`, `OTA_GITHUB_REPO`, `FIRMWARE_VERSION`, `FIRMWARE_BUILD_DATE`, `FIRMWARE_BUILD_TIME`, `OTA_DEBUG_LOGGING`, `OTA_LOG_MEMORY`, `OTA_MIN_FREE_HEAP_BYTES`
- [ ] 9.4 Update the example code at the bottom of the file to match the new API (`startBackgroundCheck` / `startBackgroundUpdateFromLatestCheck`)

## 10. Rewire WebServerManager.cpp endpoints

- [ ] 10.1 `/api/ota/check` (GET): call `OTAUpdater::startBackgroundCheck(OTA_GITHUB_OWNER, OTA_GITHUB_REPO)`. Return `202 {started: bool, error?: string}` synchronously
- [ ] 10.2 `/api/ota/status` (GET): build the new state-machine JSON: `firmware_version`, `build_date`, `partition`, `unconfirmed_update`, `free_heap`, `min_free_heap`, `ota_safe`, `check { state, version?, name?, size_bytes?, download_url?, changelog?, error? }`, `update { state, percent, bytes_written, expected_bytes, started_at_ms, error? }`
- [ ] 10.3 `/api/ota/update` (POST): read `force` from query string. Call `OTAUpdater::startBackgroundUpdateFromLatestCheck()` (force is currently ignored inside the implementation; tracked as a separate task for explicit downgrade UI). Return `202 {started: true}` or `409 {error: "..."}` synchronously
- [ ] 10.4 `/api/ota/confirm` (POST): unchanged — call `OTAUpdater::confirmBoot()` and respond with success/failure
- [ ] 10.5 Remove the body-upload handler on `/api/ota/update` (no body parsing anymore)
- [ ] 10.6 Remove the synchronous `checkForUpdate` and `performUpdate` calls from the old handlers

## 11. Add auto-confirm to Network task

- [ ] 11.1 Capture `static unsigned long bootTimeMs = millis();` at the top of `Network::task()` after the touch controller initializes
- [ ] 11.2 Add `std::atomic<bool> hasServedAnyRequest{false}` member to `WebServerManager` (or its subclasses) and a public `hasServedAnyRequest()` getter
- [ ] 11.3 Set the flag inside a small helper that runs at the top of any handler in `OperationalWebServerManager` (a single one-line `this->hasServedAnyRequest = true;` per handler, or wrapped in a `RequestTracker` RAII struct that runs first in each handler lambda)
- [ ] 11.4 In the existing 1 Hz tick block in `Network::task()`, after the NTP and timer checks: if `OTAUpdater::hasUnconfirmedUpdate()` and `(millis() - bootTimeMs) >= OTA_AUTO_CONFIRM_MIN_UPTIME_MS` and (`!OTA_AUTO_CONFIRM_REQUIRE_REQUEST || webServer->hasServedAnyRequest()`), call `OTAUpdater::confirmBoot()` and log

## 12. Rewrite settings.html UI

- [ ] 12.1 Replace the `checkForUpdates()` function: call `fetch('/api/ota/check')`, then start a `setInterval(pollOTAStatus, 1000)` loop. Stop the interval when `data.check.state !== 'in_progress'` and `data.update.state === 'idle'`
- [ ] 12.2 Replace the `performUpdate()` function: read the force checkbox, call `fetch('/api/ota/update?force=' + (force ? 'true' : 'false'), {method: 'POST'})`, then start polling. Replace the manual "alert('refresh in 30 seconds')" with the polling-driven reconnect
- [ ] 12.3 Add `pollOTAStatus()`: fetch `/api/ota/status`, render the progress bar based on `data.update.percent`, render the changelog via the new `renderReleaseNotes()` function, surface the force checkbox only when `data.check.version <= data.firmware_version`
- [ ] 12.4 Add `renderReleaseNotes(markdown)`: escape HTML, linkify URLs, replace newlines with `<br>`. ~10 lines
- [ ] 12.5 Add a real `<progress>` element (or div-based bar) showing percent + bytes, replacing the placeholder text
- [ ] 12.6 Add the labeled "Allow installing older or equal version" checkbox with a visible `id` and a small explanation tooltip
- [ ] 12.7 Update the existing `loadOTAStatus()` to populate the same fields it does today (firmware_version, partition, build_date) plus a hidden "Update available" indicator that's revealed by `pollOTAStatus`
- [ ] 12.8 Run the build to regenerate `settings.html.gz`

## 13. Add "update available" badge to control.html (optional)

- [ ] 13.1 In `loadStatus()` or a similar function, fetch `/api/ota/status` and render a small badge in the top bar if `data.check.state === 'done' && data.check.version !== data.firmware_version`
- [ ] 13.2 Badge links to `/settings` and shows `Update available: vX.Y.Z`
- [ ] 13.3 Regenerate `control.html.gz`

## 14. Add native tests

- [ ] 14.1 Create `test/test_ota/` with `test_semver.cpp`, `test_state_machine.cpp`, `test_progress.cpp`
- [ ] 14.2 `test_semver.cpp`: cases for `1.2.3` vs `1.2.4`, `1.2.4-rc.1` vs `1.2.4`, `v1.0.0` vs `1.0.0`, leading zeros, missing components, unparseable inputs (returns false vs raises)
- [ ] 14.3 `test_state_machine.cpp`: mock the HTTP layer; verify `CheckState` transitions `Idle → InProgress → Done / Failed`; verify `updateInProgress` CAS correctness under simulated concurrent calls
- [ ] 14.4 `test_progress.cpp`: verify `Progress` struct populates correctly through simulated download (mock with byte injection)
- [ ] 14.5 Add `test_ota` to `platformio.ini` `[env:native]` `test_filter` configuration
- [ ] 14.6 Run `pio test -e native -f test_ota` and confirm all tests pass; then `pio test -e native` to confirm no regressions in other suites

## 15. Add release script

- [ ] 15.1 Create `scripts/release.sh`: takes a version tag (`v1.2.3`), runs `pio run -e adafruit_qtpy_esp32s3_nopsram`, computes `sha256sum firmware.bin > firmware.bin.sha256`, runs `gh release create <tag> firmware.bin firmware.bin.sha256 --title <tag> --notes-from-stdin` (or with notes file), prints the release URL
- [ ] 15.2 Document the script in `docs/RELEASING.md` (new): "Run `./scripts/release.sh v1.2.3` from the repo root. Requires the GitHub CLI (`gh`) authenticated."
- [ ] 15.3 Add a note that until `OTA_ENABLE_SHA256_VERIFICATION` is flipped to 1, the `.sha256` file is unused but harmless to publish

## 16. Update documentation

- [ ] 16.1 Rewrite `docs/OTA_FIRMWARE_UPDATES.md` to describe the new async flow, state machine, auto-confirm, force flag, and SHA verification opt-in. Remove the klimacontrol-style HTTPClient-with-fingerprint recommendation (no longer applicable)
- [ ] 16.2 Update `docs/OTA_QUICK_START.md` to reflect the new release script and endpoints
- [ ] 16.3 Delete or rewrite `docs/OTA_IMPLEMENTATION_SUMMARY.md` (predates the new design)
- [ ] 16.4 Delete or rewrite `docs/OTA_INDEX.md` (predates the new design)
- [ ] 16.5 Delete `OTA_REFERENCE_CARD.md` and `examples/OTA_INTEGRATION_EXAMPLE.cpp` (predate the new design and would mislead)
- [ ] 16.6 Add `docs/RELEASING.md` if not created in 15.2

## 17. Build and verify

- [ ] 17.1 Run `pio run -e adafruit_qtpy_esp32s3_nopsram` and confirm the firmware compiles. Investigate any `esp_crt_bundle` link errors (usually solved by adding `-DCONFIG_ESP_CRT_BUNDLE=y` to `build_flags` in `platformio.ini` if the default doesn't pull it in)
- [ ] 17.2 Verify firmware size is under 1856 KB partition (current 988 KB + ~30 KB CA bundle + ~50 KB mbedtls_sha256 + ~10 KB new OTA code ≈ 1080 KB; 776 KB headroom)
- [ ] 17.3 Run `pio test -e native` and confirm all suites pass (rainbow-show, color, jump, smooth_blend, status_led, plus the new ota)
- [ ] 17.4 Manually flash to a real device (`pio run -e adafruit_qtpy_esp32s3_nopsram -t upload`) and verify: (a) device boots and serves web UI, (b) `/api/ota/status` returns the new JSON shape, (c) check + update flow works end-to-end with a real GitHub release, (d) auto-confirm fires after 5 minutes + first HTTP request
- [ ] 17.5 Manually verify the downgrade flow: build an older-version image, upload as a GitHub release pre-release, click "Allow installing older or equal version" on the device, verify the downgrade works
- [ ] 17.6 Manually verify the rollback safety: build a deliberately bad image (e.g., `while(true){}` in setup), flash via the new flow, reboot, wait 5 minutes, observe the device reboot twice and end up on the previous image
- [ ] 17.7 (Optional) Enable `OTA_ENABLE_SHA256_VERIFICATION = 1` in a test build, publish a release with `.sha256`, verify the verification works; tamper with the `.bin` after computing the hash, verify the worker aborts