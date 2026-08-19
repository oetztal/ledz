# ota-update Specification (delta)

## ADDED Requirements

### Requirement: TLS verification of firmware downloads

The device SHALL verify the TLS certificate of any firmware download server against a trusted CA bundle before transmitting firmware bytes to the OTA partition.

#### Scenario: GitHub release download over HTTPS

- **WHEN** the device downloads `firmware.bin` from `github.com` or `objects.githubusercontent.com`
- **THEN** the certificate presented by the server is verified against the IDF `esp_crt_bundle`
- **THEN** an invalid or self-signed certificate causes the update to abort with no bytes written to the OTA partition

#### Scenario: No insecure TLS configuration

- **WHEN** any code path in the OTA update flow creates a secure client
- **THEN** the client is configured with a CA bundle (`crt_bundle_attach` or equivalent)
- **THEN** no code path calls a function equivalent to `setInsecure()` or otherwise disables certificate chain validation

### Requirement: Asynchronous update flow

The device SHALL perform all firmware check and update work on dedicated FreeRTOS tasks, such that HTTP request handlers return to the client within a single scheduling tick regardless of network or flash latency.

#### Scenario: Check endpoint returns promptly

- **WHEN** a client sends `GET /api/ota/check`
- **THEN** the device responds within 100 ms with `{"started": true}` or `{"error": "..."}` and never blocks on the GitHub TLS handshake

#### Scenario: Update endpoint returns promptly

- **WHEN** a client sends `POST /api/ota/update`
- **THEN** the device responds within 100 ms with `{"started": true}` or `{"error": "..."}` and never blocks on the firmware download or flash write

#### Scenario: HTTP requests during update are served

- **WHEN** an OTA update worker is downloading or flashing firmware
- **THEN** other HTTP endpoints (`/api/status`, `/api/show`, `/api/brightness`, `/settings`, `/`) continue to respond within normal latency

### Requirement: Concurrent update guard

The device SHALL refuse to start a second OTA check or update while one is in progress.

#### Scenario: Second check during in-progress check

- **WHEN** `GET /api/ota/check` is called while a previous check is still running
- **THEN** the device responds with HTTP 409 and an error message indicating a check is already in progress

#### Scenario: Second update during in-progress update

- **WHEN** `POST /api/ota/update` is called while a previous update is still running
- **THEN** the device responds with HTTP 409 and an error message indicating an update is already in progress

### Requirement: Update URL origin validation

The device SHALL only flash firmware binaries whose download URL originates from the device's own GitHub release check and is hosted at `github.com`.

#### Scenario: Arbitrary URL rejected

- **WHEN** a check result contains a download URL that does not start with `https://github.com/`
- **THEN** the device refuses to start the update and sets `update.state = Failed` with an error message indicating an unexpected host

#### Scenario: Client-supplied URL is ignored

- **WHEN** a client POSTs to `/api/ota/update` with a JSON body containing `download_url`
- **THEN** the device ignores the body and uses the URL from its own most recent check result

### Requirement: Semver-aware version comparison

The device SHALL compare version tags using semantic versioning rules rather than string equality.

#### Scenario: Numeric ordering

- **WHEN** the latest version is `v1.2.4` and the current version is `v1.2.3`
- **THEN** the device reports `update_available = true`

#### Scenario: Prerelease ordering

- **WHEN** the latest version is `v1.2.4` and the current version is `v1.2.4-rc.1`
- **THEN** the device reports `update_available = true`

#### Scenario: Equal versions

- **WHEN** the latest version equals the current version per semver
- **THEN** the device reports `update_available = false` unless the client explicitly opts into downgrade with `force=true`

#### Scenario: Unparseable tag fallback

- **WHEN** one of the two versions cannot be parsed as semver and the other can
- **THEN** the device refuses the comparison and reports a `unparseable_tag` error rather than guessing

### Requirement: Forced update for downgrades and same-version reinstalls

The device SHALL support installing a firmware version that is not strictly newer than the currently running version, when the client explicitly requests it.

#### Scenario: Force flag in update request

- **WHEN** a client sends `POST /api/ota/update?force=true`
- **THEN** the device allows the update regardless of semver ordering
- **THEN** the device logs the forced downgrade in serial output

#### Scenario: Force flag absent

- **WHEN** a client sends `POST /api/ota/update` without the force flag
- **THEN** the device rejects updates where the latest version is not strictly newer than the current version
- **THEN** the device responds with HTTP 409 and an error message indicating a downgrade requires the force flag

### Requirement: Update state observation

The device SHALL expose the current OTA state via `GET /api/ota/status` so a polling client can observe progress without holding a connection open.

#### Scenario: Status returns check state

- **WHEN** a client sends `GET /api/ota/status`
- **THEN** the response contains a `check` object with a `state` field whose value is one of `idle`, `in_progress`, `done`, `failed`

#### Scenario: Status returns update state

- **WHEN** a client sends `GET /api/ota/status`
- **THEN** the response contains an `update` object with a `state` field whose value is one of `idle`, `downloading`, `flashing`, `pending`, `failed`

#### Scenario: Status returns progress percent

- **WHEN** the OTA update is in the `downloading` or `flashing` state
- **THEN** the response `update` object contains a `percent` field with an integer value between 0 and 100 inclusive

#### Scenario: Status returns bytes written

- **WHEN** the OTA update is in the `downloading` or `flashing` state
- **THEN** the response `update` object contains a `bytes_written` field with a non-decreasing value across successive polls

### Requirement: Auto-confirm after stable running

The device SHALL automatically mark a freshly-flashed firmware image as valid after a stability threshold has been met, eliminating the need for a manual confirm step.

#### Scenario: Confirm after uptime + first HTTP request

- **WHEN** the running partition is in state `ESP_OTA_IMG_NEW`
- **AND** `millis() - bootTimeMs >= OTA_AUTO_CONFIRM_MIN_UPTIME_MS` (default 300000 ms)
- **AND** at least one HTTP request has been served by the webserver
- **THEN** the device calls `esp_ota_mark_app_valid_cancel_rollback()` within the next 1-second tick
- **THEN** serial output includes `Auto-confirmed after <N> ms uptime`

#### Scenario: Manual confirm remains available

- **WHEN** a client sends `POST /api/ota/confirm`
- **THEN** the device calls `esp_ota_mark_app_valid_cancel_rollback()` immediately and returns success or failure as the JSON response

#### Scenario: Rollback still possible before confirm

- **WHEN** the running partition is in state `ESP_OTA_IMG_NEW` and the device reboots or crashes before either auto-confirm or manual confirm
- **THEN** the ESP-IDF bootloader rolls back to the previously valid partition on next boot

### Requirement: Changelog visibility

The device SHALL surface the GitHub release notes ("body") in the web UI so users can read what changed before installing an update.

#### Scenario: Changelog rendered in settings page

- **WHEN** a check completes with `check.state = done` and `check.version != firmware_version`
- **THEN** the settings page renders the release notes in a visible section
- **THEN** URLs in the release notes are rendered as clickable anchors with `target="_blank"` and `rel="noopener"`

#### Scenario: XSS-safe rendering

- **WHEN** a release body contains `<script>alert(1)</script>` or other HTML
- **THEN** the rendered output displays the literal characters as text, not as executable HTML

#### Scenario: Changelog size cap

- **WHEN** a release body exceeds 2048 bytes
- **THEN** the device truncates the body for display (with a trailing `+ … (N more bytes)` indicator if truncated) and parses the rest of the response normally

### Requirement: Optional SHA-256 verification

The device SHALL support streaming SHA-256 verification of the downloaded firmware when configured to do so, aborting the update on mismatch.

#### Scenario: SHA verification enabled and hash matches

- **WHEN** `OTA_ENABLE_SHA256_VERIFICATION = 1`
- **AND** the release publishes a `firmware.bin.sha256` file
- **THEN** the device downloads the hash file before flashing
- **THEN** the device streams the `.bin` and computes SHA-256 in parallel
- **THEN** if the computed hash matches the expected hash, the update proceeds normally

#### Scenario: SHA verification enabled and hash missing

- **WHEN** `OTA_ENABLE_SHA256_VERIFICATION = 1`
- **AND** the GitHub release does not include a `firmware.bin.sha256` file (HTTP 404)
- **THEN** the device aborts the update with `update.state = Failed` and an error message indicating the hash file is missing

#### Scenario: SHA verification enabled and hash mismatch

- **WHEN** `OTA_ENABLE_SHA256_VERIFICATION = 1`
- **AND** the computed SHA-256 of the streamed `.bin` does not match the expected hash
- **THEN** the device calls `Update.abort()` before `Update.end()`
- **THEN** `update.state = Failed` and the OTA partition is left unwritten

#### Scenario: SHA verification disabled

- **WHEN** `OTA_ENABLE_SHA256_VERIFICATION = 0`
- **THEN** the device does not fetch `firmware.bin.sha256`
- **THEN** no SHA-256 computation runs during the download
- **THEN** the update proceeds based solely on TLS verification

### Requirement: Visual signal during update

The device SHALL suspend the LED show task during firmware flashing, providing a clear visual signal that an update is in progress.

#### Scenario: LEDs go dark during update

- **WHEN** the OTA worker enters the `downloading` or `flashing` state
- **THEN** the LED show task is suspended
- **THEN** the LED strip no longer renders the active show

#### Scenario: LEDs resume on failure

- **WHEN** the OTA worker exits with `update.state = Failed`
- **THEN** the LED show task is resumed
- **THEN** the previously active show continues to render

#### Scenario: Device restart on success

- **WHEN** the OTA worker exits with `update.state = Pending`
- **THEN** `Config::ConfigManager::requestRestart(2000)` is called
- **THEN** the LED show task is not resumed (the device is about to reboot)

### Requirement: Redirect handling

The device SHALL follow HTTP redirects up to a fixed maximum hop count for both the GitHub API check and the firmware download.

#### Scenario: Standard GitHub release redirect

- **WHEN** the firmware download URL returns HTTP 302 with a `Location` header pointing to a CDN URL
- **THEN** the device follows the redirect and downloads from the CDN URL
- **THEN** the firmware bytes are written to the OTA partition

#### Scenario: Permanent and temporary redirect codes

- **WHEN** the download URL returns HTTP 301, 302, 307, or 308
- **THEN** the device follows the redirect transparently

#### Scenario: Redirect loop protection

- **WHEN** the device encounters more than 5 redirects on a single download
- **THEN** the device aborts the download with an error indicating too many redirects