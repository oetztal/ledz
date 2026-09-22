#include "WiFiCredentials.h"

#include <array>
#include <cstdio>
#include <cstring>

namespace Support {
    namespace {
        // Copy into a fixed buffer, always NUL-terminated and never overrunning.
        template <size_t N> void copyBounded(std::array<char, N>& dest, const char* src) {
            snprintf(dest.data(), dest.size(), "%s", src);
        }
    }

    Config::WiFiConfig mergeWiFiCredentials(const Config::WiFiConfig& existing, const WiFiCredentialUpdate& update) {
        Config::WiFiConfig merged = existing;

        if (update.ssid != nullptr) {
            copyBounded(merged.ssid, update.ssid);
        }

        // nullptr means the request omitted the key: keep what is stored.
        // An empty string is an explicit clear (open network).
        if (update.password != nullptr) {
            copyBounded(merged.password, update.password);
        }

        merged.configured = true;
        // New credentials invalidate the failure history; this also matches
        // the behaviour of the handler before it started from the stored
        // config, which built a fresh WiFiConfig on every save.
        merged.connection_failures = 0;

        return merged;
    }
}
