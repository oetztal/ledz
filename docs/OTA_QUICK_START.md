# OTA Quick Start

## Releasing firmware

```
./scripts/release.sh v1.2.3
```

The script builds the firmware, computes SHA-256, and creates a GitHub
release with both `firmware.bin` and `firmware.bin.sha256` as assets. See
`docs/RELEASING.md` for the full workflow.

## What the device does on every boot

1. Connects to WiFi (or starts AP mode if not configured).
2. Starts the webserver on Core 0.
3. Runs the auto-confirm check on the 1Hz tick — if the running partition
   is `ESP_OTA_IMG_NEW` (i.e. flashed but unconfirmed) AND uptime >=
   `OTA_AUTO_CONFIRM_MIN_UPTIME_MS` AND at least one HTTP request has been
   served, marks the partition valid.

## Triggering an update

There are three ways to push a firmware update:

### 1. The web UI

Visit `/settings`, click **Check for Updates**, review the changelog, and
click **Install Update**. The button shows a real progress bar driven by
`/api/ota/status` polling.

To downgrade to an older version, enable **Allow installing older or
equal version** before clicking install.

### 2. The `/api/ota/*` endpoints

```sh
# Step 1: kick off a check
curl http://ledz.local/api/ota/check -i
# HTTP/1.1 202 Accepted  {"started":true}

# Step 2: poll until check.state == done
while true; do
    curl -s http://ledz.local/api/ota/status | jq '.check.state'
    sleep 1
done

# Step 3: kick off the install
curl -X POST http://ledz.local/api/ota/update -i
# HTTP/1.1 202 Accepted  {"started":true}

# Or with force for downgrades:
curl -X POST 'http://ledz.local/api/ota/update?force=true' -i
```

### 3. Manually via serial

If you have a serial console attached, the worker logs progress in human
readable form:

```
[OTA] performUpdate: https://github.com/.../firmware.bin (1048576 bytes)
[OTA] Flashing firmware...
[OTA] Flashed 1048576 bytes
[OTA] OTA update successful - scheduling restart in 2000ms
```

## Verifying the install

After `config.requestRestart(2000)` the device reboots into the new image.
You should see:

1. LEDs go dark during the flash.
2. LEDs come back on with the new image.
3. The settings page shows the new `firmware_version` and a stable
   `partition` label.

If the new image crashes within 5 minutes of boot, the ESP-IDF bootloader
will roll back to the previous partition on the next boot — the rollback
chain is unchanged.

## Troubleshooting

### "Update refused: not newer than running"

The latest GitHub release is not strictly newer than the current firmware
per semver. Either publish a higher tag, or click the **Allow installing
older or equal version** checkbox (or use `?force=true`).

### "Update refused: url not from github.com"

The check result returned a URL that doesn't start with
`https://github.com/`. This is a security guard — the worker will never
flash firmware downloaded from an unexpected host. The check should have
filled in the right URL; check that `OTA_GITHUB_OWNER` / `OTA_GITHUB_REPO`
in `src/OTAConfig.h` match your repo.

### Device reboots twice after an update

That's the rollback chain working. The new image failed the auto-confirm
window (didn't serve an HTTP request within 5 minutes, or hit a watchdog)
and the bootloader booted the previous partition. Check the serial log
from the failed boot for a panic or assertion.

### `/api/ota/check` returns 409

Another check or update is already in progress. Either wait or check that
the previous run didn't hang — the worker tasks self-delete on completion
but a stuck TLS handshake or heap exhaustion can leave the flag set; a
reboot clears it.
