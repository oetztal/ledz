//
// OTA Firmware Update Manager Implementation
//
// Architecture:
//   - HttpClient: RAII wrapper around esp_http_client with redirect-following
//     and CA-bundle verification (no setInsecure()).
//   - EspHttpReader: streaming adapter so ArduinoJson can parse the body
//     chunk by chunk without an intermediate buffer.
//   - State machine: CheckState / UpdateState observable from Core 0.
//   - Worker tasks pinned to Core 1 so /api/ota/* handlers return in
//     microseconds and the rest of the web UI keeps serving.
//

#include "OTAUpdater.h"
#include "Config.h"
#include "OTAConfig.h"
#include "support/SemVer.h"

#include <atomic>

#ifdef ARDUINO
#include <esp_http_client.h>
#include <esp_task_wdt.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#if OTA_ENABLE_SHA256_VERIFICATION
#include <mbedtls/sha256.h>
#endif

extern "C" esp_err_t esp_crt_bundle_attach(void *conf);

// File-scope state so both the anonymous-namespace helpers and the
// OTAUpdater::* member functions (defined further down) can see it.
static std::atomic<bool> updateInProgress{false};
static CheckState checkState = CheckState::Idle;
static FirmwareInfo checkResult{};
static Progress progress{};
static Config::ConfigManager *s_config = nullptr;

static SemaphoreHandle_t checkResultMutex() {
    static SemaphoreHandle_t m = xSemaphoreCreateMutex();
    return m;
}

static SemaphoreHandle_t progressMutex() {
    static SemaphoreHandle_t m = xSemaphoreCreateMutex();
    return m;
}

namespace {

// ---------------------------------------------------------------------------
// HttpClient: RAII wrapper around esp_http_client_handle_t
// ---------------------------------------------------------------------------

struct HttpClient {
    esp_http_client_handle_t handle = nullptr;

    explicit HttpClient(const esp_http_client_config_t &config) {
        handle = esp_http_client_init(&config);
    }

    ~HttpClient() {
        if (handle) {
            esp_http_client_close(handle);
            esp_http_client_cleanup(handle);
            handle = nullptr;
        }
    }

    HttpClient(const HttpClient &) = delete;
    HttpClient &operator=(const HttpClient &) = delete;

    explicit operator bool() const { return handle != nullptr; }

    // Open the connection, following 301/302/307/308 up to `maxRedirects`.
    // Returns the final HTTP status on success, or -1 on failure.
    int openWithRedirects(int maxRedirects = 5) {
        if (!handle) return -1;
        int hops = 0;
        while (true) {
            esp_err_t err = esp_http_client_open(handle, 0);
            if (err != ESP_OK) return -1;
            int status = esp_http_client_fetch_headers(handle);
            if (status < 0) return -1;
            int code = esp_http_client_get_status_code(handle);
            if (code == 301 || code == 302 || code == 307 || code == 308) {
                if (hops >= maxRedirects) return -1;
                char *loc = nullptr;
                if (esp_http_client_get_header(handle, "Location", &loc) != ESP_OK || !loc) {
                    return -1;
                }
                String next(loc);
                free(loc);
                esp_http_client_set_url(handle, next.c_str());
                ++hops;
                continue;
            }
            return code;
        }
    }

    // Drain any remaining body bytes and close. Useful after streaming JSON
    // so the TLS buffers can be released before the caller continues.
    void drain() {
        if (!handle) return;
        char buf[256];
        while (esp_http_client_read(handle, buf, sizeof(buf)) > 0) { /* discard */ }
    }
};

// ---------------------------------------------------------------------------
// EspHttpReader: ArduinoJson::Reader adapter over esp_http_client_read
// ---------------------------------------------------------------------------

struct EspHttpReader {
    esp_http_client_handle_t handle = nullptr;
    bool eof = false;

    explicit EspHttpReader(esp_http_client_handle_t h) : handle(h) {}

    int read() {
        if (eof || !handle) return -1;
        char c;
        int n = esp_http_client_read(handle, &c, 1);
        if (n <= 0) { eof = true; return -1; }
        return static_cast<unsigned char>(c);
    }

    size_t readBytes(char *buf, size_t len) {
        if (eof || !handle) return 0;
        int n = esp_http_client_read(handle, buf, len);
        if (n <= 0) { eof = (n == 0); return 0; }
        return static_cast<size_t>(n);
    }
};

// ---------------------------------------------------------------------------
// Local helpers
// ---------------------------------------------------------------------------

struct InProgressGuard {
    bool armed;
    InProgressGuard() : armed(false) {
        bool expected = false;
        if (updateInProgress.compare_exchange_strong(expected, true)) {
            armed = true;
        }
    }
    ~InProgressGuard() { if (armed) updateInProgress.store(false); }
    bool ok() const { return armed; }
};

// ---------------------------------------------------------------------------
// Worker job structs
// ---------------------------------------------------------------------------

struct OtaCheckJob {
    char owner[64];
    char repo[64];
};

struct OtaUpdateJob {
    String url;
    size_t expected_size;
    bool force;
};

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

constexpr int HTTP_TIMEOUT_MS = 30000;
constexpr int HTTP_BUFFER_SIZE = 8192;
constexpr int HTTP_BUFFER_SIZE_TX = 2048;
constexpr int DOWNLOAD_CHUNK_SIZE = 4096;
constexpr size_t CHANGELOG_MAX_BYTES = 2048;

// ---------------------------------------------------------------------------
// SHA-256 verification context (no-op when flag is disabled)
// ---------------------------------------------------------------------------

#if OTA_ENABLE_SHA256_VERIFICATION
struct OtaVerifyContext {
    mbedtls_sha256_context ctx;
    uint8_t expected[32] = {0};
    bool hash_loaded = false;

    OtaVerifyContext() { mbedtls_sha256_init(&ctx); }
    ~OtaVerifyContext() { mbedtls_sha256_free(&ctx); }
    OtaVerifyContext(const OtaVerifyContext &) = delete;
    OtaVerifyContext &operator=(const OtaVerifyContext &) = delete;

    void update(const uint8_t *buf, size_t n) { mbedtls_sha256_update(&ctx, buf, n); }

    bool finishAndCheck() {
        uint8_t actual[32];
        mbedtls_sha256_finish(&ctx, actual);
        return memcmp(actual, expected, 32) == 0;
    }
};

bool fetchExpectedSha256(const String &url, String &outHash) {
    String hashUrl = url + OTA_SHA256_ASSET_SUFFIX;
    esp_http_client_config_t cfg{};
    cfg.url = hashUrl.c_str();
    cfg.timeout_ms = HTTP_TIMEOUT_MS;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.buffer_size = HTTP_BUFFER_SIZE;
    cfg.buffer_size_tx = HTTP_BUFFER_SIZE_TX;

    HttpClient client(cfg);
    if (!client) return false;

    int status = client.openWithRedirects();
    if (status != 200) return false;

    char line[256] = {0};
    int n = esp_http_client_read(client.handle, line, sizeof(line) - 1);
    if (n <= 0) return false;
    line[n] = '\0';
    client.drain();

    // The GitHub-uploaded .sha256 file looks like:
    //   "<hex64>   firmware.bin\n"  (sha256sum format)
    // or just "<hex64>\n". We only care about the first whitespace-delimited
    // token, lowercased.
    String raw(line);
    int sp = raw.indexOf(' ');
    int nl = raw.indexOf('\n');
    int cut = sp >= 0 ? sp : (nl >= 0 ? nl : raw.length());
    outHash = raw.substring(0, cut);
    outHash.toLowerCase();
    if (outHash.length() != 64) return false;
    for (size_t i = 0; i < outHash.length(); i++) {
        char c = outHash[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

bool parseHexSha256(const String &hex, uint8_t out[32]) {
    if (hex.length() != 64) return false;
    for (size_t i = 0; i < 32; i++) {
        auto nybble = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        int hi = nybble(hex[i * 2]);
        int lo = nybble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;
}
#endif // OTA_ENABLE_SHA256_VERIFICATION

// ---------------------------------------------------------------------------
// Private: checkForUpdate
// ---------------------------------------------------------------------------

bool doCheckForUpdate(const char *owner, const char *repo, FirmwareInfo &out) {
    out = FirmwareInfo{};

    InProgressGuard guard;
    if (!guard.ok()) {
        OTA_LOG("check refused: another OTA in progress");
        return false;
    }

    {
        SemaphoreHandle_t m = checkResultMutex();
        if (xSemaphoreTake(m, portMAX_DELAY)) {
            checkState = CheckState::InProgress;
            checkResult = FirmwareInfo{};
            xSemaphoreGive(m);
        }
    }

    String apiUrl = String("https://api.github.com/repos/") + owner + "/" + repo + "/releases/latest";

    esp_http_client_config_t cfg{};
    cfg.url = apiUrl.c_str();
    cfg.timeout_ms = HTTP_TIMEOUT_MS;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.buffer_size = HTTP_BUFFER_SIZE;
    cfg.buffer_size_tx = HTTP_BUFFER_SIZE_TX;
    cfg.user_agent = "ledz-ota/1.0";

    HttpClient client(cfg);
    if (!client) {
        SemaphoreHandle_t m = checkResultMutex();
        if (xSemaphoreTake(m, portMAX_DELAY)) {
            checkState = CheckState::Failed;
            xSemaphoreGive(m);
        }
        OTA_LOG("HTTP init failed");
        return false;
    }

    esp_http_client_set_header(client.handle, "Accept", "application/vnd.github.v3+json");

    int status = client.openWithRedirects();
    if (status != 200) {
        OTA_LOG("GitHub API returned %d (free heap: %u)", status, ESP.getFreeHeap());
        client.drain();
        SemaphoreHandle_t m = checkResultMutex();
        if (xSemaphoreTake(m, portMAX_DELAY)) {
            checkState = CheckState::Failed;
            xSemaphoreGive(m);
        }
        return false;
    }

    DynamicJsonDocument doc(Config::JSON_DOC_OTA);
    {
        EspHttpReader reader(client.handle);
        DeserializationError err = deserializeJson(doc, reader);
        client.drain();

        if (err) {
            OTA_LOG("JSON parse error: %s (heap=%u)", err.c_str(), ESP.getFreeHeap());
            SemaphoreHandle_t m = checkResultMutex();
            if (xSemaphoreTake(m, portMAX_DELAY)) {
                checkState = CheckState::Failed;
                xSemaphoreGive(m);
            }
            return false;
        }
    }

    out.version = doc["tag_name"].as<String>();
    out.name = doc["name"].as<String>();

    // Walk assets for the first .bin
    JsonArray assets = doc["assets"];
    for (JsonObject asset : assets) {
        String assetName = asset["name"].as<String>();
        if (assetName.endsWith(".bin")) {
            out.downloadUrl = asset["browser_download_url"].as<String>();
            out.size = asset["size"].as<size_t>();
            break;
        }
    }

    // Cap changelog at CHANGELOG_MAX_BYTES, with truncation marker.
    const char *body = doc["body"] | "";
    out.changelog = String(body);
    if (out.changelog.length() > CHANGELOG_MAX_BYTES) {
        size_t extra = out.changelog.length() - CHANGELOG_MAX_BYTES;
        out.changelog = out.changelog.substring(0, CHANGELOG_MAX_BYTES) +
                        "+ \xe2\x80\xa6 (" + String(extra) + " more bytes)";
    }

    doc.clear();
    doc.shrinkToFit();

    if (out.version.isEmpty() || out.downloadUrl.isEmpty() || out.size == 0) {
        OTA_LOG("No usable release found (tag=%s, url=%s, size=%zu)",
                out.version.c_str(), out.downloadUrl.c_str(), out.size);
        SemaphoreHandle_t m = checkResultMutex();
        if (xSemaphoreTake(m, portMAX_DELAY)) {
            checkState = CheckState::Failed;
            xSemaphoreGive(m);
        }
        return false;
    }

    out.isValid = true;

    SemaphoreHandle_t m = checkResultMutex();
    if (xSemaphoreTake(m, portMAX_DELAY)) {
        checkResult = out;
        checkState = CheckState::Done;
        xSemaphoreGive(m);
    }

    OTA_LOG("Check OK: %s (%s, %zu bytes, %u changelog bytes)",
            out.name.c_str(), out.version.c_str(), out.size, out.changelog.length());
    return true;
}

// ---------------------------------------------------------------------------
// Private: performUpdate (called by worker task)
// ---------------------------------------------------------------------------

bool doPerformUpdate(const String &downloadUrl, size_t expectedSize,
                     const std::function<void(int, size_t)> &onProgress) {
    OTA_LOG("performUpdate: %s (%zu bytes)", downloadUrl.c_str(), expectedSize);

    if (expectedSize == 0) {
        OTA_LOG("performUpdate: refused, expectedSize == 0");
        return false;
    }
    if (ESP.getFreeHeap() < OTA_MIN_FREE_HEAP_BYTES) {
        OTA_LOG("performUpdate: refused, heap=%u < %u", ESP.getFreeHeap(), OTA_MIN_FREE_HEAP_BYTES);
        return false;
    }

    esp_http_client_config_t cfg{};
    cfg.url = downloadUrl.c_str();
    cfg.timeout_ms = HTTP_TIMEOUT_MS;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.buffer_size = HTTP_BUFFER_SIZE;
    cfg.buffer_size_tx = HTTP_BUFFER_SIZE_TX;
    cfg.user_agent = "ledz-ota/1.0";

    HttpClient client(cfg);
    if (!client) {
        OTA_LOG("HTTP init failed");
        return false;
    }

    int status = client.openWithRedirects();
    if (status != 200) {
        OTA_LOG("download HTTP status %d (free heap: %u)", status, ESP.getFreeHeap());
        return false;
    }

    int64_t contentLen = esp_http_client_get_content_length(client.handle);
    if (contentLen > 0 && static_cast<size_t>(contentLen) != expectedSize) {
        OTA_LOG("Content-Length mismatch: header=%lld expected=%zu", contentLen, expectedSize);
        return false;
    }

    if (!Update.begin(expectedSize, U_FLASH)) {
        OTA_LOG("Update.begin failed: %s", Update.errorString());
        return false;
    }

#if OTA_ENABLE_SHA256_VERIFICATION
    OtaVerifyContext verify;
    String hashHex;
    if (!fetchExpectedSha256(downloadUrl, hashHex)) {
        OTA_LOG("SHA-256 file missing or malformed (strict mode)");
        Update.abort();
        return false;
    }
    if (!parseHexSha256(hashHex, verify.expected)) {
        OTA_LOG("SHA-256 hex parse failed");
        Update.abort();
        return false;
    }
    verify.hash_loaded = true;
    mbedtls_sha256_starts(&verify.ctx, 0); // SHA-256 (not SHA-224)
#endif

    uint8_t buffer[DOWNLOAD_CHUNK_SIZE];
    size_t totalRead = 0;
    unsigned long lastDataReceived = millis();

    while (totalRead < expectedSize) {
        int n = esp_http_client_read(client.handle, reinterpret_cast<char *>(buffer), DOWNLOAD_CHUNK_SIZE);
        if (n > 0) {
            size_t written = Update.write(buffer, n);
            if (written != static_cast<size_t>(n)) {
                OTA_LOG("Flash write failed: %d vs %d (%s)", written, n, Update.errorString());
                Update.abort();
                return false;
            }
#if OTA_ENABLE_SHA256_VERIFICATION
            verify.update(buffer, n);
#endif
            totalRead += written;
            lastDataReceived = millis();

            int percent = static_cast<int>((totalRead * 100ULL) / expectedSize);
            if (onProgress) onProgress(percent, totalRead);
            esp_task_wdt_reset();
            vTaskDelay(1);
        } else if (n == 0) {
            // EOF before expectedSize reached
            OTA_LOG("Unexpected EOF at %zu/%zu bytes", totalRead, expectedSize);
            Update.abort();
            return false;
        } else {
            // n < 0 -> error
            OTA_LOG("esp_http_client_read error: %d", n);
            Update.abort();
            return false;
        }

        // Idle-timeout watchdog
        if (millis() - lastDataReceived > 60000) {
            OTA_LOG("Download stalled (no bytes for 60s, %zu/%zu)", totalRead, expectedSize);
            Update.abort();
            return false;
        }
    }

#if OTA_ENABLE_SHA256_VERIFICATION
    if (!verify.finishAndCheck()) {
        OTA_LOG("SHA-256 mismatch - aborting");
        Update.abort();
        return false;
    }
    OTA_LOG("SHA-256 verified OK");
#endif

    if (!Update.end(false)) {
        OTA_LOG("Update.end failed: %s", Update.errorString());
        return false;
    }

    OTA_LOG("Flashed %zu bytes", totalRead);
    return true;
}

// ---------------------------------------------------------------------------
// Worker tasks
// ---------------------------------------------------------------------------

void otaCheckTask(void *arg) {
    auto *job = static_cast<OtaCheckJob *>(arg);
    char owner[sizeof(job->owner)];
    char repo[sizeof(job->repo)];
    strncpy(owner, job->owner, sizeof(owner));
    strncpy(repo, job->repo, sizeof(repo));
    delete job;

    FirmwareInfo info;
    doCheckForUpdate(owner, repo, info);

    UBaseType_t hwm = uxTaskGetStackHighWaterMark(nullptr);
    OTA_LOG("ota_check HWM=%u words", hwm);
    vTaskDelete(nullptr);
}

void otaWorkerTask(void *arg) {
    auto *job = static_cast<OtaUpdateJob *>(arg);
    String url = job->url;
    size_t expected = job->expected_size;
    bool force = job->force;
    delete job;

    // 1. Suspend LED show for visual signal + heap relief.
    TaskHandle_t showHandle = xTaskGetHandle("LedShow");
    if (showHandle) {
        vTaskSuspend(showHandle);
    }

    {
        SemaphoreHandle_t m = progressMutex();
        if (xSemaphoreTake(m, portMAX_DELAY)) {
            progress = Progress{};
            progress.state = UpdateState::Downloading;
            progress.expected_bytes = expected;
            progress.started_at_ms = millis();
            xSemaphoreGive(m);
        }
    }

    auto onProgress = [](int percent, size_t bytes) {
        OTAUpdater::publishProgress(percent, bytes);
    };

    bool ok = doPerformUpdate(url, expected, onProgress);

    if (ok) {
        SemaphoreHandle_t m = progressMutex();
        if (xSemaphoreTake(m, portMAX_DELAY)) {
            progress.state = UpdateState::Pending;
            progress.percent = 100;
            xSemaphoreGive(m);
        }
        OTA_LOG("OTA update successful - scheduling restart in 2000ms");
        if (s_config) s_config->requestRestart(2000);
    } else {
        SemaphoreHandle_t m = progressMutex();
        if (xSemaphoreTake(m, portMAX_DELAY)) {
            progress.state = UpdateState::Failed;
            progress.error_message = "flash failed";
            xSemaphoreGive(m);
        }
        OTA_LOG("OTA update failed");
    }

    // If we're heading for restart (ok path), leave the show suspended. On
    // failure, resume so the user sees a working device again.
    if (!ok && showHandle) {
        vTaskResume(showHandle);
    }

    updateInProgress.store(false);

    UBaseType_t hwm = uxTaskGetStackHighWaterMark(nullptr);
    OTA_LOG("ota_update HWM=%u words", hwm);
    vTaskDelete(nullptr);
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool OTAUpdater::startBackgroundCheck(const char *owner, const char *repo) {
    bool expected = false;
    if (updateInProgress.load()) {
        OTA_LOG("startBackgroundCheck refused: updateInProgress already true");
        return false;
    }
    {
        SemaphoreHandle_t m = checkResultMutex();
        if (xSemaphoreTake(m, portMAX_DELAY)) {
            if (checkState == CheckState::InProgress) {
                xSemaphoreGive(m);
                OTA_LOG("startBackgroundCheck refused: check already in progress");
                return false;
            }
            // We don't set checkState here - the worker task does that.
            xSemaphoreGive(m);
        }
    }

    auto *job = new OtaCheckJob{};
    strncpy(job->owner, owner, sizeof(job->owner) - 1);
    strncpy(job->repo, repo, sizeof(job->repo) - 1);

    BaseType_t rc = xTaskCreatePinnedToCore(
        otaCheckTask, "ota_check", OTA_CHECK_TASK_STACK, job, 1, nullptr, 1);
    if (rc != pdPASS) {
        delete job;
        OTA_LOG("xTaskCreate for ota_check failed");
        return false;
    }
    return true;
}

bool OTAUpdater::startBackgroundUpdateFromLatestCheck(bool force) {
    bool expected = false;
    if (!updateInProgress.compare_exchange_strong(expected, true)) {
        OTA_LOG("startBackgroundUpdate refused: updateInProgress already true");
        return false;
    }

    FirmwareInfo info;
    {
        SemaphoreHandle_t m = checkResultMutex();
        if (xSemaphoreTake(m, portMAX_DELAY)) {
            if (checkState != CheckState::Done) {
                xSemaphoreGive(m);
                updateInProgress.store(false);
                OTA_LOG("startBackgroundUpdate refused: no successful check");
                return false;
            }
            info = checkResult;
            xSemaphoreGive(m);
        } else {
            updateInProgress.store(false);
            return false;
        }
    }

    if (!info.isValid || info.size == 0 || info.downloadUrl.isEmpty()) {
        updateInProgress.store(false);
        OTA_LOG("startBackgroundUpdate refused: info incomplete");
        return false;
    }
    if (!info.downloadUrl.startsWith("https://github.com/")) {
        updateInProgress.store(false);
        OTA_LOG("startBackgroundUpdate refused: url not from github.com (%s)", info.downloadUrl.c_str());
        return false;
    }

    if (!force) {
        const std::string latest(info.version.c_str(), info.version.length());
        const std::string current(FIRMWARE_VERSION);
        if (!ota::isNewerVersion(latest, current)) {
            updateInProgress.store(false);
            OTA_LOG("startBackgroundUpdate refused: %s is not newer than %s (force=false)",
                    info.version.c_str(), FIRMWARE_VERSION);
            return false;
        }
    } else {
        OTA_LOG("Forced update: installing %s over %s", info.version.c_str(), FIRMWARE_VERSION);
    }

    auto *job = new OtaUpdateJob{info.downloadUrl, info.size, force};

    BaseType_t rc = xTaskCreatePinnedToCore(
        otaWorkerTask, "ota_update", OTA_UPDATE_TASK_STACK, job, 1, nullptr, 1);
    if (rc != pdPASS) {
        delete job;
        updateInProgress.store(false);
        OTA_LOG("xTaskCreate for ota_update failed");
        return false;
    }
    return true;
}

CheckState OTAUpdater::getCheckState() {
    SemaphoreHandle_t m = checkResultMutex();
    CheckState s = CheckState::Idle;
    if (xSemaphoreTake(m, portMAX_DELAY)) {
        s = checkState;
        xSemaphoreGive(m);
    }
    return s;
}

FirmwareInfo OTAUpdater::getCheckResult() {
    SemaphoreHandle_t m = checkResultMutex();
    FirmwareInfo r;
    if (xSemaphoreTake(m, portMAX_DELAY)) {
        r = checkResult;
        xSemaphoreGive(m);
    }
    return r;
}

Progress OTAUpdater::getProgress() {
    SemaphoreHandle_t m = progressMutex();
    Progress p;
    if (xSemaphoreTake(m, portMAX_DELAY)) {
        p = progress;
        xSemaphoreGive(m);
    }
    return p;
}

bool OTAUpdater::isUpdateInProgress() {
    return updateInProgress.load();
}

void OTAUpdater::setConfig(Config::ConfigManager *cfg) {
    s_config = cfg;
}

void OTAUpdater::publishProgress(int percent, size_t bytes) {
    SemaphoreHandle_t m = progressMutex();
    if (xSemaphoreTake(m, portMAX_DELAY)) {
        progress.percent = static_cast<uint8_t>(percent > 100 ? 100 : percent);
        progress.bytes_written = bytes;
        if (progress.state == UpdateState::Downloading && percent >= 100) {
            progress.state = UpdateState::Flashing;
        }
        xSemaphoreGive(m);
    }
}

void OTAUpdater::otaCheckTaskEntry(void *arg) { otaCheckTask(arg); }
void OTAUpdater::otaWorkerTaskEntry(void *arg) { otaWorkerTask(arg); }

bool OTAUpdater::checkForUpdate(const char *owner, const char *repo, FirmwareInfo &info) {
    return doCheckForUpdate(owner, repo, info);
}

bool OTAUpdater::performUpdate(const String &downloadUrl, size_t expectedSize,
                               const std::function<void(int, size_t)> &onProgress) {
    return doPerformUpdate(downloadUrl, expectedSize, onProgress);
}

// ---------------------------------------------------------------------------
// Boot confirmation (unchanged)
// ---------------------------------------------------------------------------

bool OTAUpdater::confirmBoot() {
    OTA_LOG("Confirming OTA boot");
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err != ESP_OK) {
        OTA_LOG("Boot confirmation failed: %s", esp_err_to_name(err));
        return false;
    }
    OTA_LOG("Boot confirmed - rollback disabled");
    return true;
}

bool OTAUpdater::hasUnconfirmedUpdate() {
    const esp_partition_t *runningPartition = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    esp_err_t err = esp_ota_get_state_partition(runningPartition, &state);
    if (err != ESP_OK) return false;
    return (state == ESP_OTA_IMG_NEW);
}

bool OTAUpdater::getRunningPartitionInfo(String &label, uint32_t &address) {
    const esp_partition_t *runningPartition = esp_ota_get_running_partition();
    if (runningPartition == nullptr) return false;
    label = String(runningPartition->label);
    address = runningPartition->address;
    return true;
}

void OTAUpdater::getMemoryInfo(uint32_t &freeHeap, uint32_t &minFreeHeap, uint32_t &psramFree) {
    freeHeap = esp_get_free_heap_size();
    minFreeHeap = esp_get_minimum_free_heap_size();
#ifdef CONFIG_SPIRAM
    psramFree = esp_get_free_psram_size();
#else
    psramFree = 0;
#endif
}

bool OTAUpdater::hasEnoughMemory() {
    uint32_t freeHeap, minFree, psramFree;
    getMemoryInfo(freeHeap, minFree, psramFree);
    if (freeHeap < OTA_MIN_FREE_HEAP_BYTES) return false;
    return true;
}

#endif // ARDUINO
