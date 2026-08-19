# Releasing a new firmware version

Publishing a new ledz release is one command:

```
./scripts/release.sh v1.2.3
```

The script will:

1. Build the firmware with `pio run -e adafruit_qtpy_esp32s3_nopsram`.
2. Compute the SHA-256 of `firmware.bin` and emit `firmware.bin.sha256`.
3. Run `gh release create <tag>` with both files as assets, plus a default
   release-notes body (`"Release v1.2.3"`) and the tag as title.

The tag is the only required argument. Optional flags:

- `--prerelease` — mark the release as a pre-release on GitHub.
- `--publish` — by default the release is created as a draft; pass `--publish`
  to publish it directly.
- `--notes "text"` — set the release body. If omitted and stdin is not a
  TTY (e.g., inside a pipe), notes are read from stdin.

## Prerequisites

- `gh` (GitHub CLI) installed and authenticated (`gh auth login`).
- `pio` (PlatformIO) installed and able to build for the
  `adafruit_qtpy_esp32s3_nopsram` environment.

## SHA-256 verification

Currently `OTA_ENABLE_SHA256_VERIFICATION` defaults to `0` in
`src/OTAConfig.h`. The `.sha256` file is published but unused by the device.
To opt in:

1. Flip `OTA_ENABLE_SHA256_VERIFICATION` to `1` in `src/OTAConfig.h`.
2. Rebuild and flash the firmware to all deployed devices (one-time cost;
   afterwards every release auto-uses the `.sha256` file).
3. From that release forward, the device refuses to install any update where
   the computed SHA-256 doesn't match the published hash. Strict mode: a
   missing `.sha256` file aborts the update.

The release script already publishes the hash for every release, so opting
in is a one-line change plus a single re-flash.

## Versioning

`ledz` follows [semver 2.0.0](https://semver.org/). The OTA update logic
compares tags via a hand-rolled comparator (see `src/support/SemVer.h`) that
honours prerelease ordering (`v1.2.4-rc.1 < v1.2.4`).

To downgrade to an older or equal version, the device must be told via the
`force` query parameter on `/api/ota/update`:

```
POST /api/ota/update?force=true
```

The settings page surfaces a checkbox for that flag whenever the latest
release is not strictly newer than the running version.
