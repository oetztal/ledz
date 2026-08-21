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
#include "Log.h"
#include "OTAConfig.h"
#include "support/SemVer.h"

#include <atomic>
#include <memory>
#include <new>

static const char* TAG = "ota";

#ifdef ARDUINO
#include <esp_http_client.h>
#include <esp_task_wdt.h>
#include <esp_tls.h>  // esp_tls_get_and_clear_last_error() for TLS diagnostic codes
#include <esp_sntp.h>  // esp_sntp_restart() / sntp_set_sync_mode(IMMED)
#include <WiFi.h>
#include <esp_netif.h>
#include <lwip/netdb.h>   // getaddrinfo() — the resolver esp-tls itself uses
#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <esp_system.h>   // esp_reset_reason() — is the clock this boot's, or RTC carry-over?
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <Arduino.h>  // getLocalTime() for SNTP sync wait

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

    // Diagnostic state captured during the last attempt. Surfaced to callers
    // so the failure log can name the actual esp_err_t / socket errno
    // instead of the opaque -1 callers used to see.
    esp_err_t lastOpenErr = ESP_OK;
    int lastHeadersResult = 0;
    int lastSocketErrno = 0;
    int lastStatusCode = 0;

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

    // Capture the underlying socket errno and surface it on failure.
    // `esp_http_client_get_errno()` is the only public diagnostic the
    // transport layer exposes; it directly reports lwIP `SO_ERROR` values
    // (EHOSTUNREACH=113, ETIMEDOUT=110, ECONNREFUSED=111, ...), which is
    // far more useful than the opaque -1 callers used to see.
    void captureSocketErrno() {
        if (!handle) return;
        lastSocketErrno = esp_http_client_get_errno(handle);
    }

    // Open the connection, following 301/302/307/308 up to `maxRedirects`.
    // Returns the final HTTP status on success, or -1 on failure. On failure,
    // detailed state is captured into the `last*` fields so the caller can
    // log a precise reason.
    int openWithRedirects(int maxRedirects = 5) {
        if (!handle) return -1;
        int hops = 0;
        while (true) {
            lastOpenErr = esp_http_client_open(handle, 0);
            if (lastOpenErr != ESP_OK) {
                captureSocketErrno();
                ESP_LOGE(TAG, "HTTP open failed: %s (errno=%d)",
                        esp_err_to_name(lastOpenErr), lastSocketErrno);
                return -1;
            }
            lastHeadersResult = esp_http_client_fetch_headers(handle);
            if (lastHeadersResult < 0) {
                captureSocketErrno();
                ESP_LOGE(TAG, "HTTP fetch_headers failed: %d (errno=%d, esp_err=%s)",
                        lastHeadersResult, lastSocketErrno,
                        esp_err_to_name(lastOpenErr));
                return -1;
            }
            lastStatusCode = esp_http_client_get_status_code(handle);
            int code = lastStatusCode;
            if (code == 301 || code == 302 || code == 307 || code == 308) {
                if (hops >= maxRedirects) {
                    ESP_LOGE(TAG, "HTTP too many redirects (last status=%d)", code);
                    return -1;
                }
                // Close the existing connection BEFORE changing the URL.
                // Without this, esp_http_client_open() returns ESP_OK because
                // the state machine still thinks it's connected, but the
                // subsequent fetch_headers() reads the old response and
                // returns the stale redirect status again (infinite loop).
                esp_http_client_close(handle);
                // esp_http_client_get_header() reads the *request* headers —
                // there is no public getter for a response header, so asking
                // it for "Location" always comes back empty. The parsed
                // Location lives in the client's private state and the only
                // way at it is set_redirection(), which points the client at
                // it (resolving relative targets against the current URL).
                if (esp_http_client_set_redirection(handle) != ESP_OK) {
                    ESP_LOGE(TAG, "HTTP missing Location header on %d", code);
                    return -1;
                }
                if (esp_log_level_get(TAG) >= ESP_LOG_DEBUG) {
                    char next[256] = {0};
                    if (esp_http_client_get_url(handle, next, sizeof(next)) == ESP_OK) {
                        ESP_LOGD(TAG, "HTTP %d redirect -> %s", code, next);
                    }
                }
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

#ifdef ARDUINO
// ---------------------------------------------------------------------------
// Name-resolution diagnostics
//
// A failing OTA check bottoms out in one of three very different faults, and
// the resolver's own error code (EAI_FAIL for all of them) cannot tell them
// apart:
//   1. lwIP has no usable DNS server configured,
//   2. lwIP cannot allocate a UDP socket for the query (PCB/fd exhaustion) —
//      fails instantly and stays broken for the lifetime of the boot,
//   3. the query goes out but no answer comes back (router filtering, or the
//      datagram lost on a marginal link) — fails after ~10 s of retries.
// These helpers pin down which one, so we stop guessing.
// ---------------------------------------------------------------------------

// Encode `name` as a DNS QNAME (length-prefixed labels, zero terminator).
// Returns bytes written, or 0 if the name doesn't fit / has an empty label.
size_t encodeQName(const char *name, uint8_t *buf, size_t cap) {
    size_t n = 0;
    for (const char *p = name; *p;) {
        const char *dot = strchr(p, '.');
        size_t len = dot ? static_cast<size_t>(dot - p) : strlen(p);
        if (len == 0 || len > 63 || n + len + 2 > cap) return 0;
        buf[n++] = static_cast<uint8_t>(len);
        memcpy(buf + n, p, len);
        n += len;
        p = dot ? dot + 1 : p + len;
    }
    if (n + 1 > cap) return 0;
    buf[n++] = 0;  // root label
    return n;
}

struct RawDnsResult {
    enum class Outcome { Answered, SocketFailed, SendFailed, NoReply, Malformed } outcome;
    int err = 0;         // errno for the socket/send/recv failures
    int rcode = -1;      // DNS RCODE (0 = NOERROR, 3 = NXDOMAIN, 5 = REFUSED)
    int answerCount = 0;
    uint32_t elapsedMs = 0;
};

// Send a hand-built DNS A query straight to `server` over UDP/53 and wait for
// the reply. This is ground truth: it shares nothing with lwIP's resolver, so
// an answer here while getaddrinfo() fails means the resolver is at fault,
// and a timeout here means the query never made it back through the network.
RawDnsResult rawDnsQuery(uint32_t serverIp4, const char *name, uint32_t timeoutMs) {
    RawDnsResult r{RawDnsResult::Outcome::Answered};
    uint32_t start = millis();

    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        r.outcome = RawDnsResult::Outcome::SocketFailed;
        r.err = errno;
        r.elapsedMs = millis() - start;
        return r;
    }
    struct timeval tv{};
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    uint8_t query[128];
    query[0] = 0x4c; query[1] = 0x5a;  // transaction id ("LZ")
    query[2] = 0x01; query[3] = 0x00;  // standard query, recursion desired
    query[4] = 0x00; query[5] = 0x01;  // qdcount = 1
    memset(query + 6, 0, 6);           // ancount / nscount / arcount = 0
    size_t n = 12;
    size_t qn = encodeQName(name, query + n, sizeof(query) - n - 4);
    if (qn == 0) {
        close(fd);
        r.outcome = RawDnsResult::Outcome::Malformed;
        return r;
    }
    n += qn;
    query[n++] = 0x00; query[n++] = 0x01;  // qtype = A
    query[n++] = 0x00; query[n++] = 0x01;  // qclass = IN

    struct sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(53);
    dst.sin_addr.s_addr = serverIp4;
    if (sendto(fd, query, n, 0, reinterpret_cast<struct sockaddr *>(&dst), sizeof(dst)) < 0) {
        r.outcome = RawDnsResult::Outcome::SendFailed;
        r.err = errno;
        close(fd);
        r.elapsedMs = millis() - start;
        return r;
    }

    uint8_t reply[256];
    int got = recvfrom(fd, reply, sizeof(reply), 0, nullptr, nullptr);
    close(fd);
    r.elapsedMs = millis() - start;
    if (got < 0) {
        r.outcome = RawDnsResult::Outcome::NoReply;
        r.err = errno;
        return r;
    }
    if (got < 12) {
        r.outcome = RawDnsResult::Outcome::Malformed;
        return r;
    }
    r.rcode = reply[3] & 0x0f;
    r.answerCount = (reply[6] << 8) | reply[7];
    return r;
}

// Resolve `host` through the same resolver esp-tls uses, logging the outcome
// and how long it took. Elapsed time is the tell: instant failure means lwIP
// never got a query out, ~10 s means it retried and gave up waiting.
bool probeGetAddrInfo(const char *host, int family, const char *familyName) {
    struct addrinfo hints{};
    hints.ai_family = family;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res = nullptr;
    uint32_t start = millis();
    int gai = getaddrinfo(host, "443", &hints, &res);
    uint32_t elapsed = millis() - start;

    if (gai == 0 && res) {
        char ipstr[INET6_ADDRSTRLEN] = "?";
        if (res->ai_family == AF_INET6) {
            auto *sa = reinterpret_cast<struct sockaddr_in6 *>(res->ai_addr);
            inet_ntop(AF_INET6, &sa->sin6_addr, ipstr, sizeof(ipstr));
        } else {
            auto *sa = reinterpret_cast<struct sockaddr_in *>(res->ai_addr);
            inet_ntop(AF_INET, &sa->sin_addr, ipstr, sizeof(ipstr));
        }
        ESP_LOGD(TAG, "OTA diag: getaddrinfo(%s, %s) -> %s in %lu ms",
                host, familyName, ipstr, (unsigned long)elapsed);
        freeaddrinfo(res);
        return true;
    }
    if (res) freeaddrinfo(res);
    ESP_LOGD(TAG, "OTA diag: getaddrinfo(%s, %s) FAILED gai=%d after %lu ms",
            host, familyName, gai, (unsigned long)elapsed);
    return false;
}

// Returns true if the server actually answered (whatever the RCODE).
bool logRawDnsQuery(uint32_t serverIp4, const char *host, const char *serverLabel) {
    RawDnsResult raw = rawDnsQuery(serverIp4, host, 4000);
    switch (raw.outcome) {
        case RawDnsResult::Outcome::Answered:
            ESP_LOGD(TAG, "OTA diag: raw DNS A? %s @%s -> rcode=%d answers=%d in %lu ms",
                    host, serverLabel, raw.rcode, raw.answerCount,
                    (unsigned long)raw.elapsedMs);
            break;
        case RawDnsResult::Outcome::NoReply:
            ESP_LOGD(TAG, "OTA diag: raw DNS A? %s @%s got NO REPLY in %lu ms (errno=%d)",
                    host, serverLabel, (unsigned long)raw.elapsedMs, raw.err);
            break;
        case RawDnsResult::Outcome::SocketFailed:
            ESP_LOGD(TAG, "OTA diag: raw DNS A? %s @%s could not open a socket (errno=%d)",
                    host, serverLabel, raw.err);
            break;
        case RawDnsResult::Outcome::SendFailed:
            ESP_LOGD(TAG, "OTA diag: raw DNS A? %s @%s sendto failed (errno=%d) — no route",
                    host, serverLabel, raw.err);
            break;
        case RawDnsResult::Outcome::Malformed:
            ESP_LOGD(TAG, "OTA diag: raw DNS A? %s @%s reply malformed/too short",
                    host, serverLabel);
            break;
    }
    return raw.outcome == RawDnsResult::Outcome::Answered;
}

// Non-blocking TCP connect with an explicit timeout. Returns true if the peer
// is *reachable*, which includes an outright refusal: ECONNREFUSED means the
// host answered and merely has the port closed. Only a timeout means nothing
// came back at all, and EHOSTUNREACH means we have no route.
bool logTcpProbe(uint32_t ip4, uint16_t port, const char *label) {
    if (ip4 == 0) return false;
    uint32_t start = millis();
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        ESP_LOGD(TAG, "OTA diag: tcp %s socket() failed (errno=%d)", label, errno);
        return false;
    }
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(port);
    dst.sin_addr.s_addr = ip4;

    int rc = connect(fd, reinterpret_cast<struct sockaddr *>(&dst), sizeof(dst));
    if (rc == 0) {
        ESP_LOGD(TAG, "OTA diag: tcp %s connected immediately (%lu ms)",
                label, (unsigned long)(millis() - start));
        close(fd);
        return true;
    }
    if (errno != EINPROGRESS) {
        ESP_LOGD(TAG, "OTA diag: tcp %s connect failed at once (errno=%d)", label, errno);
        close(fd);
        return false;
    }

    fd_set wset;
    FD_ZERO(&wset);
    FD_SET(fd, &wset);
    struct timeval tv{};
    tv.tv_sec = 5;
    int sel = select(fd + 1, nullptr, &wset, nullptr, &tv);
    uint32_t elapsed = millis() - start;
    bool reachable = false;
    if (sel > 0) {
        int soErr = 0;
        socklen_t len = sizeof(soErr);
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &soErr, &len);
        if (soErr == 0) {
            ESP_LOGD(TAG, "OTA diag: tcp %s CONNECTED in %lu ms (host reachable)",
                    label, (unsigned long)elapsed);
            reachable = true;
        } else {
            reachable = (soErr == ECONNREFUSED);
            ESP_LOGD(TAG, "OTA diag: tcp %s refused/failed in %lu ms (SO_ERROR=%d)%s",
                    label, (unsigned long)elapsed, soErr,
                    reachable ? " — host is reachable, port closed" : "");
        }
    } else if (sel == 0) {
        ESP_LOGD(TAG, "OTA diag: tcp %s TIMED OUT after %lu ms — no response at all",
                label, (unsigned long)elapsed);
    } else {
        ESP_LOGD(TAG, "OTA diag: tcp %s select() failed (errno=%d)", label, errno);
    }
    close(fd);
    return reachable;
}

// Probe the network after a failed request and print one actionable verdict.
// Runs only on the failure path — the probes cost up to ~35 s, which is fine
// when the check has already failed but must never delay a working update.
//
// `noinline` is load-bearing, not a hint: inlined into doCheckForUpdate this
// function's probe buffers and log scratch space become part of that frame and
// stay live for the whole call, stealing ~1.5 KB from the mbedtls handshake
// that runs long before any of this code is reached.
__attribute__((noinline))
void explainNetworkFailure(const char *host) {
    // Force IPv4 rather than AF_UNSPEC: AF_UNSPEC makes lwIP walk an
    // A-then-AAAA sequence, so a plain IPv4 probe isolates the A lookup. If
    // resolution works, the fault is downstream of DNS (TLS handshake, CA
    // bundle, cert validity) and none of the network probes below apply.
    if (probeGetAddrInfo(host, AF_INET, "AF_INET")) {
        ESP_LOGW(TAG, "OTA verdict: %s resolves, so the failure is in the TLS/HTTP "
                "layer, not the network — check the CA bundle and the clock",
                host);
        return;
    }

    // What lwIP's resolver itself is configured with. WiFi.dnsIP() reads the
    // same table, but log all DNS_MAX_SERVERS slots: a populated slot 0 with
    // the query still failing rules out "no server configured" outright.
    uint32_t server0 = 0;
    {
        char servers[64] = {0};
        size_t off = 0;
        for (uint8_t i = 0; i < 3; ++i) {
            IPAddress s = WiFi.dnsIP(i);
            if (i == 0) server0 = static_cast<uint32_t>(s);
            off += snprintf(servers + off, sizeof(servers) - off, "%s%s",
                            i ? "," : "", s.toString().c_str());
            if (off >= sizeof(servers)) break;
        }
        ESP_LOGD(TAG, "OTA diag: resolvers=[%s] reset_reason=%d sntp_sync=%d",
                servers, (int)esp_reset_reason(), (int)sntp_get_sync_status());
    }

    // Can we get a UDP socket at all? lwIP allocates one per DNS query, and
    // if sockets or UDP PCBs are exhausted every lookup fails instantly and
    // permanently — a fault that looks exactly like a dead DNS server.
    bool socketsOk = true;
    int probe = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (probe < 0) {
        socketsOk = false;
        ESP_LOGD(TAG, "OTA diag: UDP socket allocation FAILED (errno=%d)", errno);
    } else {
        close(probe);
    }

    // Raw queries bypassing lwIP's resolver entirely, then TCP reachability.
    // ECONNREFUSED counts as reachable, so only a timeout means the path is
    // dead. Gateway-vs-internet is the split that matters: the gateway
    // answering while the internet does not is a router that has this device
    // on a no-forwarding policy, and no amount of resolver configuration can
    // work around it.
    const uint32_t publicDns = inet_addr(NET_FALLBACK_DNS_1);
    bool routerAnswers = server0 != 0 && logRawDnsQuery(server0, host, "configured resolver");
    bool publicAnswers = logRawDnsQuery(publicDns, host, NET_FALLBACK_DNS_1);
    bool gatewayReachable = logTcpProbe(static_cast<uint32_t>(WiFi.gatewayIP()), 53, "gateway:53");
    bool internetReachable = logTcpProbe(publicDns, 53, NET_FALLBACK_DNS_1 ":53");

    if (!socketsOk) {
        ESP_LOGW(TAG, "OTA verdict: the network stack is out of sockets, so no lookup "
                "can be sent. This is a firmware socket leak, not a network fault.");
    } else if (!gatewayReachable) {
        ESP_LOGW(TAG, "OTA verdict: gateway %s does not respond even over TCP. The WiFi "
                "link is associated but there is no working path to the router.",
                WiFi.gatewayIP().toString().c_str());
    } else if (!internetReachable) {
        // The case seen in the field: LAN fine, nothing forwarded upstream.
        ESP_LOGW(TAG, "OTA verdict: gateway %s is reachable but nothing upstream is — "
                "this device has no internet access. Check the router's access "
                "policy (parental controls / MAC allow-list / guest isolation) "
                "for %s (%s). Nothing in the firmware can work around it.",
                WiFi.gatewayIP().toString().c_str(),
                WiFi.localIP().toString().c_str(), WiFi.macAddress().c_str());
    } else if (!routerAnswers && !publicAnswers) {
        ESP_LOGW(TAG, "OTA verdict: TCP works upstream but no DNS server answers over "
                "UDP/53 — UDP/53 egress is filtered on this network.");
    } else {
        ESP_LOGW(TAG, "OTA verdict: a DNS server answers directly, yet lwIP's resolver "
                "still fails. Suspect the resolver configuration, not the network.");
    }
}
#endif // ARDUINO

// ---------------------------------------------------------------------------
// Private: checkForUpdate
// ---------------------------------------------------------------------------

bool doCheckForUpdate(const char *owner, const char *repo, FirmwareInfo &out) {
    out = FirmwareInfo{};

    InProgressGuard guard;
    if (!guard.ok()) {
        ESP_LOGW(TAG, "check refused: another OTA in progress");
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

#ifdef ARDUINO
    // Common failure mode on real devices: the user clicks "Check for
    // updates" while the device is still in AP setup mode (no upstream
    // link) or the STA link has dropped. The TLS handshake then fails
    // with EHOSTUNREACH or ETIMEDOUT; rather than burying that as a
    // generic -1, surface it explicitly.
    wl_status_t wifi = WiFi.status();
    if (wifi != WL_CONNECTED) {
        ESP_LOGD(TAG, "GitHub API check skipped: WiFi not connected (status=%d, mode=%d, heap=%u)",
                (int)wifi, (int)WiFi.getMode(), ESP.getFreeHeap());
        SemaphoreHandle_t m = checkResultMutex();
        if (xSemaphoreTake(m, portMAX_DELAY)) {
            checkState = CheckState::Failed;
            xSemaphoreGive(m);
        }
        return false;
    }

    // mbedtls X.509 verification reads through time()/gettimeofday(). If the
    // system clock is still at boot-RTC (≈ Jan 1970 + uptime), GitHub's leaf
    // certs — which are only valid from 2024+ — look "not yet valid" and the
    // TLS handshake aborts. Surface it explicitly so we don't have to guess.
    // 1700000000 = 2023-11-14. Anything past 2023 is the modern web's floor.
    {
        // Wait for lwip SNTP (started by configTime() in
        // Network::setupUsingSTAMode) to deliver a time. SNTP's first
        // round-trip on a cold boot can easily take 20-30 s while DNS
        // resolves and the UDP/123 packet makes its way out, so we give it
        // generous headroom. If it still hasn't arrived we force a fresh
        // attempt (esp_sntp_restart) and wait again — that's the difference
        // between "the user's network is too locked-down to ever serve NTP"
        // and "the first sync just hadn't completed yet".
        auto waitForSync = [](uint32_t ms) -> bool {
            struct tm out{};
            return getLocalTime(&out, ms);
        };

        bool synced = waitForSync(30000);
        if (!synced && esp_sntp_enabled()) {
            ESP_LOGD(TAG, "OTA check: SNTP first attempt did not complete within 30s — forcing fresh sync");
            esp_sntp_restart();
            synced = waitForSync(20000);
        }

        if (!synced) {
            time_t sys_time = time(nullptr);
            ESP_LOGW(TAG, "OTA check aborted: system clock not synced after 50s total "
                    "(epoch=%ld). lwip SNTP never delivered. UDP/123 or NTP server "
                    "DNS is likely blocked on this network; OTA over HTTPS cannot work.",
                    (long)sys_time);
            SemaphoreHandle_t m = checkResultMutex();
            if (xSemaphoreTake(m, portMAX_DELAY)) {
                checkState = CheckState::Failed;
                xSemaphoreGive(m);
            }
            return false;
        }
        time_t sys_time = time(nullptr);
        struct tm timeinfo;
        gmtime_r(&sys_time, &timeinfo);
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
        ESP_LOGD(TAG, "OTA diag: rssi=%d ch=%d ip=%s sys_time=%s (%ld)",
                (int)WiFi.RSSI(), (int)WiFi.channel(), WiFi.localIP().toString().c_str(),
                buf, (long)sys_time);
    }

    ESP_LOGD(TAG, "OTA diag: dns=%s gw=%s",
            WiFi.dnsIP().toString().c_str(), WiFi.gatewayIP().toString().c_str());
#endif

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
        ESP_LOGE(TAG, "HTTP init failed");
        return false;
    }

    esp_http_client_set_header(client.handle, "Accept", "application/vnd.github.v3+json");

    int status = client.openWithRedirects();
    if (status != 200) {
        // EINPROGRESS is the in-flight state of a non-blocking connect, not a
        // fault, so don't present it as the reason for the failure.
        if (client.lastSocketErrno == 0 || client.lastSocketErrno == EINPROGRESS) {
            ESP_LOGW(TAG, "GitHub API returned %d (free heap: %u)", status, ESP.getFreeHeap());
        } else {
            ESP_LOGW(TAG, "GitHub API returned %d (free heap: %u, errno=%d)",
                    status, ESP.getFreeHeap(), client.lastSocketErrno);
        }
        client.drain();
        SemaphoreHandle_t m = checkResultMutex();
        if (xSemaphoreTake(m, portMAX_DELAY)) {
            checkState = CheckState::Failed;
            xSemaphoreGive(m);
        }
#ifdef ARDUINO
        // Only now, with the request already failed, is it worth spending ~35 s
        // probing the network to say *why* in one actionable line.
        explainNetworkFailure("api.github.com");
#endif
        return false;
    }

    DynamicJsonDocument doc(Config::JSON_DOC_OTA);
    {
        EspHttpReader reader(client.handle);
        DeserializationError err = deserializeJson(doc, reader);
        client.drain();

        if (err) {
            ESP_LOGW(TAG, "JSON parse error: %s (heap=%u)", err.c_str(), ESP.getFreeHeap());
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
        ESP_LOGW(TAG, "No usable release found (tag=%s, url=%s, size=%zu)",
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

    ESP_LOGI(TAG, "Check OK: %s (%s, %zu bytes, %u changelog bytes)",
            out.name.c_str(), out.version.c_str(), out.size, out.changelog.length());
    return true;
}

// ---------------------------------------------------------------------------
// Private: performUpdate (called by worker task)
// ---------------------------------------------------------------------------

bool doPerformUpdate(const String &downloadUrl, size_t expectedSize,
                     const std::function<void(int, size_t)> &onProgress) {
    ESP_LOGI(TAG, "performUpdate: %s (%zu bytes)", downloadUrl.c_str(), expectedSize);

    if (expectedSize == 0) {
        ESP_LOGW(TAG, "performUpdate: refused, expectedSize == 0");
        return false;
    }
    if (ESP.getFreeHeap() < OTA_MIN_FREE_HEAP_BYTES) {
        ESP_LOGW(TAG, "performUpdate: refused, heap=%u < %u", ESP.getFreeHeap(), OTA_MIN_FREE_HEAP_BYTES);
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
        ESP_LOGE(TAG, "HTTP init failed");
        return false;
    }

    int status = client.openWithRedirects();
    if (status != 200) {
        ESP_LOGW(TAG, "download HTTP status %d (free heap: %u)", status, ESP.getFreeHeap());
        return false;
    }

    int64_t contentLen = esp_http_client_get_content_length(client.handle);
    if (contentLen > 0 && static_cast<size_t>(contentLen) != expectedSize) {
        ESP_LOGW(TAG, "Content-Length mismatch: header=%lld expected=%zu", contentLen, expectedSize);
        return false;
    }

    if (!Update.begin(expectedSize, U_FLASH)) {
        ESP_LOGE(TAG, "Update.begin failed: %s", Update.errorString());
        return false;
    }

#if OTA_ENABLE_SHA256_VERIFICATION
    OtaVerifyContext verify;
    String hashHex;
    if (!fetchExpectedSha256(downloadUrl, hashHex)) {
        ESP_LOGE(TAG, "SHA-256 file missing or malformed (strict mode)");
        Update.abort();
        return false;
    }
    if (!parseHexSha256(hashHex, verify.expected)) {
        ESP_LOGE(TAG, "SHA-256 hex parse failed");
        Update.abort();
        return false;
    }
    verify.hash_loaded = true;
    mbedtls_sha256_starts(&verify.ctx, 0); // SHA-256 (not SHA-224)
#endif

    // Heap, not stack: a 4 KB automatic buffer here would sit in this frame for
    // the whole download, and the TLS session it shares the task stack with
    // needs that headroom more than we do.
    std::unique_ptr<uint8_t[]> buffer(new (std::nothrow) uint8_t[DOWNLOAD_CHUNK_SIZE]);
    if (!buffer) {
        ESP_LOGE(TAG, "download buffer allocation failed (heap=%u)", ESP.getFreeHeap());
        Update.abort();
        return false;
    }
    size_t totalRead = 0;
    unsigned long lastDataReceived = millis();

    while (totalRead < expectedSize) {
        int n = esp_http_client_read(client.handle, reinterpret_cast<char *>(buffer.get()), DOWNLOAD_CHUNK_SIZE);
        if (n > 0) {
            size_t written = Update.write(buffer.get(), n);
            if (written != static_cast<size_t>(n)) {
                ESP_LOGE(TAG, "Flash write failed: %d vs %d (%s)", written, n, Update.errorString());
                Update.abort();
                return false;
            }
#if OTA_ENABLE_SHA256_VERIFICATION
            verify.update(buffer.get(), n);
#endif
            totalRead += written;
            lastDataReceived = millis();

            int percent = static_cast<int>((totalRead * 100ULL) / expectedSize);
            if (onProgress) onProgress(percent, totalRead);
            esp_task_wdt_reset();
            vTaskDelay(1);
        } else if (n == 0) {
            // EOF before expectedSize reached
            ESP_LOGW(TAG, "Unexpected EOF at %zu/%zu bytes", totalRead, expectedSize);
            Update.abort();
            return false;
        } else {
            // n < 0 -> error
            ESP_LOGE(TAG, "esp_http_client_read error: %d", n);
            Update.abort();
            return false;
        }

        // Idle-timeout watchdog
        if (millis() - lastDataReceived > 60000) {
            ESP_LOGW(TAG, "Download stalled (no bytes for 60s, %zu/%zu)", totalRead, expectedSize);
            Update.abort();
            return false;
        }
    }

#if OTA_ENABLE_SHA256_VERIFICATION
    if (!verify.finishAndCheck()) {
        ESP_LOGE(TAG, "SHA-256 mismatch - aborting");
        Update.abort();
        return false;
    }
    ESP_LOGI(TAG, "SHA-256 verified OK");
#endif

    if (!Update.end(false)) {
        ESP_LOGE(TAG, "Update.end failed: %s", Update.errorString());
        return false;
    }

    ESP_LOGI(TAG, "Flashed %zu bytes", totalRead);
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

    // Logged at I, not D: CORE_DEBUG_LEVEL is 0 in the release build, so a D
    // line would be compiled out — and this is the one number that tells us
    // whether the stack budget is still sound.
    UBaseType_t hwm = uxTaskGetStackHighWaterMark(nullptr);
    ESP_LOGI(TAG, "ota_check stack headroom left: %u bytes of %u", hwm, OTA_CHECK_TASK_STACK);
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
        ESP_LOGI(TAG, "OTA update successful - scheduling restart in 2000ms");
        if (s_config) s_config->requestRestart(2000);
    } else {
        SemaphoreHandle_t m = progressMutex();
        if (xSemaphoreTake(m, portMAX_DELAY)) {
            progress.state = UpdateState::Failed;
            progress.error_message = "flash failed";
            xSemaphoreGive(m);
        }
        ESP_LOGE(TAG, "OTA update failed");
    }

    // If we're heading for restart (ok path), leave the show suspended. On
    // failure, resume so the user sees a working device again.
    if (!ok && showHandle) {
        vTaskResume(showHandle);
    }

    updateInProgress.store(false);

    UBaseType_t hwm = uxTaskGetStackHighWaterMark(nullptr);
    ESP_LOGI(TAG, "ota_update stack headroom left: %u bytes of %u", hwm, OTA_UPDATE_TASK_STACK);
    vTaskDelete(nullptr);
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool OTAUpdater::startBackgroundCheck(const char *owner, const char *repo) {
    bool expected = false;
    if (updateInProgress.load()) {
        ESP_LOGW(TAG, "startBackgroundCheck refused: updateInProgress already true");
        return false;
    }
    {
        SemaphoreHandle_t m = checkResultMutex();
        if (xSemaphoreTake(m, portMAX_DELAY)) {
            if (checkState == CheckState::InProgress) {
                xSemaphoreGive(m);
                ESP_LOGW(TAG, "startBackgroundCheck refused: check already in progress");
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
        ESP_LOGE(TAG, "xTaskCreate for ota_check failed");
        return false;
    }
    return true;
}

bool OTAUpdater::startBackgroundUpdateFromLatestCheck(bool force) {
    bool expected = false;
    if (!updateInProgress.compare_exchange_strong(expected, true)) {
        ESP_LOGW(TAG, "startBackgroundUpdate refused: updateInProgress already true");
        return false;
    }

    FirmwareInfo info;
    {
        SemaphoreHandle_t m = checkResultMutex();
        if (xSemaphoreTake(m, portMAX_DELAY)) {
            if (checkState != CheckState::Done) {
                xSemaphoreGive(m);
                updateInProgress.store(false);
                ESP_LOGW(TAG, "startBackgroundUpdate refused: no successful check");
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
        ESP_LOGW(TAG, "startBackgroundUpdate refused: info incomplete");
        return false;
    }
    if (!info.downloadUrl.startsWith("https://github.com/")) {
        updateInProgress.store(false);
        ESP_LOGW(TAG, "startBackgroundUpdate refused: url not from github.com (%s)", info.downloadUrl.c_str());
        return false;
    }

    if (!force) {
        const std::string latest(info.version.c_str(), info.version.length());
        const std::string current(FIRMWARE_VERSION);
        if (!ota::isNewerVersion(latest, current)) {
            updateInProgress.store(false);
            ESP_LOGW(TAG, "startBackgroundUpdate refused: %s is not newer than %s (force=false)",
                    info.version.c_str(), FIRMWARE_VERSION);
            return false;
        }
    } else {
        ESP_LOGI(TAG, "Forced update: installing %s over %s", info.version.c_str(), FIRMWARE_VERSION);
    }

    auto *job = new OtaUpdateJob{info.downloadUrl, info.size, force};

    BaseType_t rc = xTaskCreatePinnedToCore(
        otaWorkerTask, "ota_update", OTA_UPDATE_TASK_STACK, job, 1, nullptr, 1);
    if (rc != pdPASS) {
        delete job;
        updateInProgress.store(false);
        ESP_LOGE(TAG, "xTaskCreate for ota_update failed");
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
    ESP_LOGI(TAG, "Confirming OTA boot");
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Boot confirmation failed: %s", esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "Boot confirmed - rollback disabled");
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
