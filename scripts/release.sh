#!/usr/bin/env bash
#
# scripts/release.sh - publish a GitHub release with the firmware.bin and
# firmware.bin.sha256 assets that ledz's OTA updater expects.
#
# Usage:
#   ./scripts/release.sh v1.2.3 [--prerelease] [--notes "Release notes"]
#
# The version tag is the only required argument. Anything that isn't passed
# in --notes is read from stdin. The release is published as a draft unless
# --publish is also supplied.

set -euo pipefail

if [ $# -lt 1 ]; then
    echo "Usage: $0 <tag> [--prerelease] [--publish] [--notes 'text']" >&2
    echo "       (release notes can also be piped via stdin)" >&2
    exit 2
fi

TAG="$1"
shift

PRERELEASE_FLAG=""
PUBLISH_FLAG=""
NOTES=""
while [ $# -gt 0 ]; do
    case "$1" in
        --prerelease) PRERELEASE_FLAG="--prerelease" ;;
        --publish)    PUBLISH_FLAG="--draft=false" ;; # explicit publish
        --notes)      NOTES="$2"; shift ;;
        *) echo "Unknown flag: $1" >&2; exit 2 ;;
    esac
    shift
done

REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "$REPO_ROOT"

if [ -z "$NOTES" ] && [ ! -t 0 ]; then
    NOTES="$(cat)"
fi
if [ -z "$NOTES" ]; then
    NOTES="Release $TAG"
fi

echo "==> Building firmware for adafruit_qtpy_esp32s3_nopsram"
pio run -e adafruit_qtpy_esp32s3_nopsram

FW=".pio/build/adafruit_qtpy_esp32s3_nopsram/firmware.bin"
if [ ! -f "$FW" ]; then
    echo "Build did not produce $FW" >&2
    exit 1
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cp "$FW" "$TMP/firmware.bin"
( cd "$TMP" && sha256sum firmware.bin > firmware.bin.sha256 )
echo "==> SHA-256"
cat "$TMP/firmware.bin.sha256"

if ! command -v gh >/dev/null 2>&1; then
    echo "gh (GitHub CLI) is required. brew install gh" >&2
    exit 1
fi
if ! gh auth status >/dev/null 2>&1; then
    echo "gh is not authenticated. Run 'gh auth login' first." >&2
    exit 1
fi

EXTRA_FLAGS=("$PRERELEASE_FLAG")
if [ -n "$PUBLISH_FLAG" ]; then
    EXTRA_FLAGS+=("$PUBLISH_FLAG")
fi

echo "==> Creating release $TAG"
gh release create "$TAG" \
    "$TMP/firmware.bin" \
    "$TMP/firmware.bin.sha256" \
    --title "$TAG" \
    --notes "$NOTES" \
    "${EXTRA_FLAGS[@]}"

echo "Done. Release URL:"
gh release view "$TAG" --json url -q .url

echo
echo "Note: until OTA_ENABLE_SHA256_VERIFICATION is flipped to 1 in"
echo "      src/OTAConfig.h, the firmware.bin.sha256 file is unused but"
echo "      harmless to publish."
