//
// OTA Firmware Update Manager for ESP32-S3
//
// Architecture:
//   - TLS-verified GitHub release check, performed off the AsyncTCP thread.
//   - State machine (`CheckState`, `UpdateState`) observable via /api/ota/status.
//   - Background workers pinned to Core 1 so /api/ota/* handlers return in
//     microseconds and the rest of the web UI keeps serving during the
//     multi-minute download.
//

#pragma once

#include <Arduino.h>
#include <functional>

namespace Config {
    class ConfigManager;
}

#ifdef ARDUINO
#include <Update.h>
#include <esp_ota_ops.h>
#endif

// ---------------------------------------------------------------------------
// Public types
// ---------------------------------------------------------------------------

enum class CheckState : uint8_t {
    Idle = 0,
    InProgress = 1,
    Done = 2,
    Failed = 3,
};

enum class UpdateState : uint8_t {
    Idle = 0,
    Downloading = 1,
    Flashing = 2,
    Pending = 3, // flashed; awaiting reboot
    Failed = 4,
};

struct FirmwareInfo {
    String version;        // Tag name (e.g., "v1.2.4")
    String name;           // Release name
    String downloadUrl;    // Direct download URL to .bin (must be github.com)
    size_t size = 0;       // File size in bytes
    String changelog;      // Release notes body (capped at 2 KB by parser)
    bool isValid = false;  // True if the struct contains valid data
};

struct Progress {
    UpdateState state = UpdateState::Idle;
    uint8_t percent = 0;              // 0-100
    size_t bytes_written = 0;
    size_t expected_bytes = 0;
    unsigned long started_at_ms = 0; // millis() when this run started
    String error_message;             // populated on Failed
};

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

class OTAUpdater {
public:
    // ---- Background workers ------------------------------------------------

    // Spawn `otaCheckTask` on Core 1. Refuses if a check is already running
    // or if a download/flash is in progress.
    // Returns true if the worker was created.
    static bool startBackgroundCheck(const char *owner, const char *repo);

    // Spawn `otaWorkerTask` on Core 1 using the URL/size from the most recent
    // successful `getCheckResult()`. `force = true` bypasses the semver
    // newer-than check (downgrades, reinstalls).
    // Returns true if the worker was created.
    static bool startBackgroundUpdateFromLatestCheck(bool force = false);

    // ---- State observers ---------------------------------------------------

    static CheckState getCheckState();
    static FirmwareInfo getCheckResult(); // returns a copy
    static Progress getProgress();
    static bool isUpdateInProgress();

    // ---- Boot confirmation (unchanged semantics) --------------------------

    static bool confirmBoot();
    static bool hasUnconfirmedUpdate();

    // ---- Configuration dependency ------------------------------------------

    /**
     * Inject the global ConfigManager so the OTA worker can call
     * `requestRestart()` after a successful flash. Must be called once
     * during setup() before any update completes.
     */
    static void setConfig(Config::ConfigManager *cfg);

    // ---- Partition + memory introspection (unchanged) ---------------------

    static bool getRunningPartitionInfo(String &label, uint32_t &address);
    static void getMemoryInfo(uint32_t &freeHeap, uint32_t &minFreeHeap, uint32_t &psramFree);
    static bool hasEnoughMemory();

    // ---- Worker-task entry points (exposed for xTaskCreatePinnedToCore) ----

    struct CheckJob { char owner[64]; char repo[64]; };
    struct UpdateJob { String url; size_t expected_size; bool force; };

    static void otaCheckTaskEntry(void *arg);
    static void otaWorkerTaskEntry(void *arg);

    static void publishProgress(int percent, size_t bytes);

private:
    // ---- Private helpers (used by worker tasks) ---------------------------

    static bool checkForUpdate(const char *owner, const char *repo, FirmwareInfo &out);
    static bool performUpdate(const String &downloadUrl, size_t expectedSize,
                              const std::function<void(int, size_t)> &onProgress);
};
