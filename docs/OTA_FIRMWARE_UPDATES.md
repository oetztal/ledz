# ledz OTA Firmware Updates

Firmware updates are delivered as GitHub releases. The device checks for
new releases, downloads the `.bin` asset with TLS verification, and writes
it to the inactive OTA partition. After flashing, the new image is
auto-confirmed once the device has been up for a few minutes and has served
at least one HTTP request.

## Architecture overview

```
+------------------+        +--------------------+        +-----------------+
|  Settings page   |  HTTP  |  Network task      |  queue |  OTA workers    |
|  (browser)       +------->+  (Core 0)          +------->+  (Core 1)       |
|                  |        |  /api/ota/*        |        |  ota_check      |
+------------------+        |  returns 202       |        |  ota_update     |
                            +---------+-----------+        +--------+--------+
                                      |                             |
                                      | AsyncTCP stays free         | TLS stream
                                      | to serve other requests     | to GitHub
                                      v                             v
                            +--------------------+        +-----------------+
                            |  Web UI keeps      |        |  OTA partition  |
                            |  responding        |        |  (app0/app1)    |
                            +--------------------+        +-----------------+
```

Key properties:

- **TLS is actually verified.** The download client uses
  `esp_http_client` with `esp_crt_bundle_attach`, so the GitHub certificate
  is validated against Mozilla's CA bundle shipped in the IDF. There is no
  `setInsecure()` call anywhere in the firmware path.
- **HTTP handlers return in microseconds.** `GET /api/ota/check` and
  `POST /api/ota/update` immediately spawn FreeRTOS tasks pinned to Core 1
  and return `202 {started: true}`. The rest of the web UI keeps responding
  normally for the entire multi-minute download.
- **Visual signal.** While the worker is downloading or flashing, the LED
  show task is suspended — the strip goes dark for the duration. When the
  device reboots into the new image, the LEDs come back on; that's the
  confirmation.
- **Semver-aware version comparison.** `v1.2.4-rc.1 < v1.2.4 <
  v1.2.4+build.7`. Downgrades require `?force=true` on `/api/ota/update`.

## State machines

Two independent state machines are observable via `/api/ota/status`:

```
CheckState:  Idle --start--> InProgress --ok--> Done
                                          \--fail--> Failed

UpdateState: Idle --start--> Downloading --complete--> Flashing --ok--> Pending
                                                       \--fail----> Failed
```

The check can finish (Done or Failed) without an update being started; the
update can be in flight while no check is happening.

`updateInProgress` is an atomic flag set during *both* the check and the
update. Concurrent `/api/ota/check` or `/api/ota/update` calls during an
in-flight operation receive HTTP 409.

## Endpoints

| Method | Path                | Behavior                                  |
|--------|---------------------|-------------------------------------------|
| GET    | `/api/ota/check`    | Spawn `ota_check` on Core 1, return 202   |
| POST   | `/api/ota/update?force=true` | Spawn `ota_update`, return 202     |
| GET    | `/api/ota/status`   | Snapshot of the state machines + memory   |
| POST   | `/api/ota/confirm`  | Manual boot confirmation (escape hatch)   |

### `/api/ota/check` — start a background check

```http
GET /api/ota/check HTTP/1.1
```
```http
HTTP/1.1 202 Accepted
Content-Type: application/json
{"started": true}
```

While the check is running, `/api/ota/status.check.state` reads
`in_progress`. When it finishes, the state transitions to `done` (or
`failed`) and `check.version`, `check.size_bytes`, `check.download_url`
and `check.changelog` are populated for the polling client.

### `/api/ota/update?force=true|false` — start a background install

```http
POST /api/ota/update HTTP/1.1
```
```http
HTTP/1.1 202 Accepted
Content-Type: application/json
{"started": true}
```

The handler does NOT read a body. The download URL is taken from the most
recent successful check result. The worker refuses URLs that don't start
with `https://github.com/`.

`force=true` bypasses the semver "must be strictly newer" check; the device
will install the latest release even if it is older than, or equal to, the
running version. The worker logs `forced downgrade` to serial.

### `/api/ota/status` — state snapshot

```json
{
  "firmware_version": "v1.2.4",
  "build_date": "Mar 12 2026",
  "build_time": "10:42:13",
  "partition": "app0",
  "unconfirmed_update": false,
  "free_heap": 152000,
  "min_free_heap": 134000,
  "ota_safe": true,
  "check": {
    "state": "done",
    "version": "v1.2.5",
    "name": "Release v1.2.5",
    "size_bytes": 1048576,
    "download_url": "https://github.com/.../firmware.bin",
    "changelog": "Bug fixes..."
  },
  "update": {
    "state": "downloading",
    "percent": 42,
    "bytes_written": 440401,
    "expected_bytes": 1048576,
    "started_at_ms": 1234567
  }
}
```

The settings page polls this endpoint once per second while a check or
update is in flight.

## Auto-confirm

Once the new image is running and the device has been up for
`OTA_AUTO_CONFIRM_MIN_UPTIME_MS` (default 5 minutes) AND has served at
least one HTTP request (controlled by `OTA_AUTO_CONFIRM_REQUIRE_REQUEST`),
the Network task calls `esp_ota_mark_app_valid_cancel_rollback()` in its
1Hz tick. From that point on, the device will not roll back to the
previous image on reboot.

If you want to confirm immediately (or you're running a headless device
with the request gate disabled), the manual `/api/ota/confirm` endpoint
remains available.

## Optional SHA-256 verification

The default policy is TLS-only. To additionally verify the firmware bytes
against a published hash:

1. Set `OTA_ENABLE_SHA256_VERIFICATION 1` in `src/OTAConfig.h`.
2. Make sure every release publishes a `firmware.bin.sha256` next to
   `firmware.bin`. The release script (`scripts/release.sh`) does this
   automatically.
3. Rebuild and reflash every device one final time.

After that, the device:
- Downloads `<asset_url>.sha256` before the firmware.
- Streams the `.bin` while computing SHA-256 in parallel.
- Aborts the update with `update.state = Failed` if the hash doesn't match
  or the `.sha256` file is missing (strict mode).

The mbedtls_sha256 working set adds ~184 bytes of stack and ~2 KB of flash;
no heap overhead. mbedtls is already linked for TLS.

## Memory budget

The OTA worker needs at least `OTA_MIN_FREE_HEAP_BYTES` (64 KB) free at
startup. If less is available, `/api/ota/update` returns 409 and the
existing LED show keeps running.

The check task needs ~7 KB of stack; the update task needs ~10 KB. Both
are well within the S3's free heap budget. The high-water mark of each
worker is logged at task exit (`ota_check HWM=…`, `ota_update HWM=…`) so
the values can be tuned in a follow-up if needed.

## Partition table

The partition layout is unchanged from the pre-change version:

```
app0 (1,856 KB) - Primary firmware partition
app1 (1,856 KB) - OTA target partition
otadata (8 KB)  - Boot partition selector with rollback support
nvs (20 KB)     - Configuration storage
spiffs (256 KB) - Web assets (future use)
coredump (64 KB)- Crash diagnostics
```

The CA bundle adds ~30 KB flash; current 988 KB usage leaves ~830 KB of
headroom in the firmware partition.

## See also

- `docs/RELEASING.md` — release workflow
- `src/OTAUpdater.h` — public API reference
- `src/support/SemVer.h` — semver comparator (tested natively)
- `openspec/changes/2026-08-19-ota-update-rework/` — the design proposal
