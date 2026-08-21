#include "WiFiCredentials.h"

#include <cstring>

namespace Support {
    namespace {
        // strncpy into a fixed buffer, always NUL-terminated.
        template<size_t N>
        void copyBounded(char (&dest)[N], const char *src) {
            strncpy(dest, src, N - 1);
            dest[N - 1] = '\0';
        }
    }

    Config::WiFiConfig mergeWiFiCredentials(const Config::WiFiConfig &existing,
                                            const WiFiCredentialUpdate &update) {
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
