//
// OTA Configuration
//
// All OTA behavior is controlled through the #defines in this file.
// The actual implementation lives in OTAUpdater.{h,cpp}.
//

#pragma once

// ============================================================================
// GitHub Configuration
// ============================================================================

#define OTA_GITHUB_OWNER "oetztal"
#define OTA_GITHUB_REPO "ledz"

// ============================================================================
// Firmware Version
// ============================================================================

// Firmware version is injected at build time from git tag via scripts/get_version.py
// If not building with PlatformIO (e.g., manual compilation), fallback to this version
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "v0.0.0-dev"
#endif

// Build timestamp (for diagnostics)
#define FIRMWARE_BUILD_DATE __DATE__
#define FIRMWARE_BUILD_TIME __TIME__

// ============================================================================
// Auto-Confirm Policy
// ============================================================================

// The freshly-flashed image is auto-confirmed once the device has been up
// for at least this many milliseconds AND has served at least one HTTP
// request. This replaces the old "set OTA_AUTO_CONFIRM_DELAY_MS to enable"
// model with a stability threshold that catches boot-crash regressions.
#define OTA_AUTO_CONFIRM_MIN_UPTIME_MS 300000  // 5 minutes

// If non-zero, the device must have served at least one HTTP request before
// auto-confirming. Set to 0 for headless deployments where no web UI is used.
#define OTA_AUTO_CONFIRM_REQUIRE_REQUEST 1

// ============================================================================
// SHA-256 Verification (opt-in)
// ============================================================================

// 0: TLS-only verification (current production policy).
// 1: additionally download <asset>.sha256 and stream-verify the firmware.
//    Requires every release to publish a `.sha256` asset; otherwise the
//    update fails strict-mode with "hash file missing".
#define OTA_ENABLE_SHA256_VERIFICATION 0

// Suffix appended to the firmware asset URL when fetching the hash file.
#define OTA_SHA256_ASSET_SUFFIX ".sha256"

// ============================================================================
// Task Stack Sizes
// ============================================================================

// Stack size for the OTA check worker (FreeRTOS words, i.e. bytes on ESP32).
#define OTA_CHECK_TASK_STACK 7168

// Stack size for the OTA update worker (TLS + Update.write working set).
#define OTA_UPDATE_TASK_STACK 10240

// ============================================================================
// Memory and Heap Budgets
// ============================================================================

// Minimum free heap required before starting an OTA worker (bytes).
#define OTA_MIN_FREE_HEAP_BYTES 65536  // 64 KB

// ============================================================================
// Logging
// ============================================================================

#define OTA_DEBUG_LOGGING 1
#define OTA_LOG_MEMORY 1

// ============================================================================
// Derived Configuration
// ============================================================================

#ifdef OTA_DEBUG_LOGGING
#define OTA_LOG(fmt, ...) Serial.printf("[OTA] " fmt "\n", ##__VA_ARGS__)
#else
#define OTA_LOG(fmt, ...)
#endif

// ============================================================================
// Example: How to Use These Settings
// ============================================================================

/*
In your code:

#include "OTAConfig.h"
#include "OTAUpdater.h"

// In setup(), after WiFi is up:
OTAUpdater::startBackgroundCheck(OTA_GITHUB_OWNER, OTA_GITHUB_REPO);

// In a web handler:
bool started = OTAUpdater::startBackgroundUpdateFromLatestCheck();
if (!started) { /* 409 conflict *\/ }

// To force a downgrade:
bool started = OTAUpdater::startBackgroundUpdateFromLatestCheck(true);
*/
