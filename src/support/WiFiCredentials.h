//
// WiFi credential merge rule for POST /api/settings/wifi
//
// Extracted to src/support/ so the native test environment can unit-test it:
// WebServerManager.cpp is excluded from the native build_src_filter and pulls
// in ArduinoJson / ESPAsyncWebServer, so the handler itself is untestable
// there. Config::WiFiConfig is native-safe (Config.h guards only
// ConfigManager behind #ifdef ARDUINO), so the rule can live here unmocked.
//
// The rule exists because the settings page prefills the SSID but can never
// prefill the password: a blank password field must mean "keep the stored
// one", or a user who opens settings and presses Update loses their
// credentials and locks themselves out of the device.
//

#ifndef LEDZ_WIFI_CREDENTIALS_H
#define LEDZ_WIFI_CREDENTIALS_H

#include "../Config.h"

namespace Support {
    /**
     * A parsed WiFi credential update request.
     *
     * The distinction between a null and an empty password is load-bearing:
     * an absent JSON key preserves the stored password, while a present but
     * empty one clears it (an open network).
     */
    struct WiFiCredentialUpdate {
        const char *ssid = nullptr;     // required; nullptr/empty is rejected by the caller
        const char *password = nullptr; // nullptr == key absent from the request body
    };

    /**
     * Merge a credential update onto the stored configuration.
     *
     * @param existing The currently stored configuration
     * @param update   The requested change
     * @return The configuration to persist: SSID replaced, password replaced
     *         only when update.password is non-null, configured set to true.
     *         Both strings are truncated to fit their buffers.
     */
    Config::WiFiConfig mergeWiFiCredentials(const Config::WiFiConfig &existing,
                                            const WiFiCredentialUpdate &update);
}

#endif // LEDZ_WIFI_CREDENTIALS_H
