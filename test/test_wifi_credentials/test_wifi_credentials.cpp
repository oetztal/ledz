#include "unity.h"
#include "support/WiFiCredentials.h"

#include <cstring>

using Support::WiFiCredentialUpdate;
using Support::mergeWiFiCredentials;

namespace {
    Config::WiFiConfig storedConfig(const char *ssid, const char *password) {
        Config::WiFiConfig config;
        strncpy(config.ssid, ssid, sizeof(config.ssid) - 1);
        config.ssid[sizeof(config.ssid) - 1] = '\0';
        strncpy(config.password, password, sizeof(config.password) - 1);
        config.password[sizeof(config.password) - 1] = '\0';
        config.configured = true;
        return config;
    }
}

// The core rule: a blank password field on the settings page omits the key,
// and must not wipe working credentials.
void test_absent_password_is_preserved() {
    const Config::WiFiConfig existing = storedConfig("HomeNet", "secret");

    WiFiCredentialUpdate update;
    update.ssid = "GuestNet";
    update.password = nullptr;

    const Config::WiFiConfig merged = mergeWiFiCredentials(existing, update);

    TEST_ASSERT_EQUAL_STRING("GuestNet", merged.ssid);
    TEST_ASSERT_EQUAL_STRING("secret", merged.password);
}

void test_empty_password_clears() {
    const Config::WiFiConfig existing = storedConfig("HomeNet", "secret");

    WiFiCredentialUpdate update;
    update.ssid = "CafeWiFi";
    update.password = "";

    const Config::WiFiConfig merged = mergeWiFiCredentials(existing, update);

    TEST_ASSERT_EQUAL_STRING("CafeWiFi", merged.ssid);
    TEST_ASSERT_EQUAL_STRING("", merged.password);
}

void test_non_empty_password_replaces() {
    const Config::WiFiConfig existing = storedConfig("HomeNet", "secret");

    WiFiCredentialUpdate update;
    update.ssid = "HomeNet";
    update.password = "newsecret";

    const Config::WiFiConfig merged = mergeWiFiCredentials(existing, update);

    TEST_ASSERT_EQUAL_STRING("HomeNet", merged.ssid);
    TEST_ASSERT_EQUAL_STRING("newsecret", merged.password);
}

void test_ssid_replaced_password_untouched_when_only_ssid_changes() {
    const Config::WiFiConfig existing = storedConfig("HomeNet", "secret");

    WiFiCredentialUpdate update;
    update.ssid = "HomeNet-5G";

    const Config::WiFiConfig merged = mergeWiFiCredentials(existing, update);

    TEST_ASSERT_EQUAL_STRING("HomeNet-5G", merged.ssid);
    TEST_ASSERT_EQUAL_STRING("secret", merged.password);
}

void test_configured_flag_is_set() {
    Config::WiFiConfig existing; // default-constructed: configured == false
    TEST_ASSERT_FALSE(existing.configured);

    WiFiCredentialUpdate update;
    update.ssid = "HomeNet";
    update.password = "secret";

    const Config::WiFiConfig merged = mergeWiFiCredentials(existing, update);

    TEST_ASSERT_TRUE(merged.configured);
}

// Saving new credentials starts the failure history over. This also matches
// the pre-change handler, which built a fresh WiFiConfig on every save.
void test_connection_failures_reset() {
    Config::WiFiConfig existing = storedConfig("HomeNet", "secret");
    existing.connection_failures = 7;

    WiFiCredentialUpdate update;
    update.ssid = "GuestNet";

    const Config::WiFiConfig merged = mergeWiFiCredentials(existing, update);

    TEST_ASSERT_EQUAL_UINT8(0, merged.connection_failures);
}

void test_oversized_inputs_are_truncated_and_terminated() {
    const Config::WiFiConfig existing = storedConfig("HomeNet", "secret");

    // Both buffers are char[64]; feed them more than they can hold.
    char longSsid[200];
    memset(longSsid, 'a', sizeof(longSsid) - 1);
    longSsid[sizeof(longSsid) - 1] = '\0';

    char longPassword[200];
    memset(longPassword, 'b', sizeof(longPassword) - 1);
    longPassword[sizeof(longPassword) - 1] = '\0';

    WiFiCredentialUpdate update;
    update.ssid = longSsid;
    update.password = longPassword;

    const Config::WiFiConfig merged = mergeWiFiCredentials(existing, update);

    TEST_ASSERT_EQUAL_size_t(sizeof(merged.ssid) - 1, strlen(merged.ssid));
    TEST_ASSERT_EQUAL_size_t(sizeof(merged.password) - 1, strlen(merged.password));
    TEST_ASSERT_EQUAL_CHAR('\0', merged.ssid[sizeof(merged.ssid) - 1]);
    TEST_ASSERT_EQUAL_CHAR('\0', merged.password[sizeof(merged.password) - 1]);
}

// Defensive: the handler rejects a missing SSID with 400 before calling the
// merge, but the helper should not corrupt the stored SSID if it ever does.
void test_null_ssid_preserves_stored_ssid() {
    const Config::WiFiConfig existing = storedConfig("HomeNet", "secret");

    WiFiCredentialUpdate update; // both fields nullptr

    const Config::WiFiConfig merged = mergeWiFiCredentials(existing, update);

    TEST_ASSERT_EQUAL_STRING("HomeNet", merged.ssid);
    TEST_ASSERT_EQUAL_STRING("secret", merged.password);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_absent_password_is_preserved);
    RUN_TEST(test_empty_password_clears);
    RUN_TEST(test_non_empty_password_replaces);
    RUN_TEST(test_ssid_replaced_password_untouched_when_only_ssid_changes);
    RUN_TEST(test_configured_flag_is_set);
    RUN_TEST(test_connection_failures_reset);
    RUN_TEST(test_oversized_inputs_are_truncated_and_terminated);
    RUN_TEST(test_null_ssid_preserves_stored_ssid);
    return UNITY_END();
}
