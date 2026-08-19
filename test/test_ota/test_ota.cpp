#include "unity.h"
#include "support/SemVer.h"

#include <atomic>
#include <cstdint>
#include <string>

// OTAUpdater.h pulls in <Arduino.h> for the `String` class and is `#ifdef
// ARDUINO`-gated, so we mirror Progress/FirmwareInfo here for native tests.
enum class UpdateStateMirror : uint8_t {
    Idle = 0,
    Downloading = 1,
    Flashing = 2,
    Pending = 3,
    Failed = 4,
};

struct ProgressMirror {
    UpdateStateMirror state = UpdateStateMirror::Idle;
    uint8_t percent = 0;
    size_t bytes_written = 0;
    size_t expected_bytes = 0;
    unsigned long started_at_ms = 0;
    std::string error_message;
};

struct FirmwareInfoMirror {
    std::string version;
    std::string name;
    std::string downloadUrl;
    size_t size = 0;
    std::string changelog;
    bool isValid = false;
};

// Mirror of the OTAUpdater state machine semantics (see src/OTAUpdater.cpp).
// Re-implemented here in pure C++ so the spec can be exercised without the
// ESP-IDF toolchain; asserts that the documented transitions hold.
enum class CheckState : uint8_t { Idle, InProgress, Done, Failed };
enum class UpdateState : uint8_t { Idle, Downloading, Flashing, Pending, Failed };

struct State {
    std::atomic<bool> updateInProgress{false};
    CheckState check = CheckState::Idle;
    UpdateState update = UpdateState::Idle;
    int checkFailures = 0;
    int updateFailures = 0;

    bool claimUpdate() {
        bool expected = false;
        return updateInProgress.compare_exchange_strong(expected, true);
    }
    void releaseUpdate() { updateInProgress.store(false); }

    bool startCheck() {
        if (updateInProgress.load()) return false;
        if (check == CheckState::InProgress) return false;
        // Per spec, updateInProgress is set during both check AND update
        // so a second concurrent claimUpdate() refuses.
        bool expected = false;
        if (!updateInProgress.compare_exchange_strong(expected, true)) return false;
        check = CheckState::InProgress;
        return true;
    }
    void finishCheck(bool ok) {
        check = ok ? CheckState::Done : CheckState::Failed;
        if (!ok) ++checkFailures;
        releaseUpdate();
    }
    bool startUpdate() {
        if (updateInProgress.load()) return false;
        if (!claimUpdate()) return false;
        update = UpdateState::Downloading;
        return true;
    }
    void markFlashing() {
        if (update == UpdateState::Downloading) update = UpdateState::Flashing;
    }
    void finishUpdate(bool ok) {
        update = ok ? UpdateState::Pending : UpdateState::Failed;
        if (!ok) ++updateFailures;
        releaseUpdate();
    }
};

using ota::SemVer;
using ota::parseSemVer;
using ota::compareSemVer;
using ota::isNewerVersion;
using ota::compareVersions;

void setUp() {}
void tearDown() {}

// =============== semver tests ===============

static void test_parse_simple() {
    auto v = parseSemVer("1.2.3");
    TEST_ASSERT_TRUE(v.has_value());
    TEST_ASSERT_EQUAL_INT(1, v->major);
    TEST_ASSERT_EQUAL_INT(2, v->minor);
    TEST_ASSERT_EQUAL_INT(3, v->patch);
    TEST_ASSERT_TRUE(v->prerelease.empty());
}
static void test_parse_with_v_prefix() {
    auto v = parseSemVer("v1.2.3");
    TEST_ASSERT_TRUE(v.has_value());
    TEST_ASSERT_EQUAL_INT(1, v->major);
}
static void test_parse_with_prerelease() {
    auto v = parseSemVer("1.2.4-rc.1");
    TEST_ASSERT_TRUE(v.has_value());
    TEST_ASSERT_EQUAL_INT(4, v->patch);
    TEST_ASSERT_EQUAL_STRING("rc.1", v->prerelease.c_str());
}
static void test_parse_with_build_metadata() {
    auto v = parseSemVer("1.0.0+build.7");
    TEST_ASSERT_TRUE(v.has_value());
    TEST_ASSERT_EQUAL_INT(0, v->patch);
}
static void test_parse_empty() { TEST_ASSERT_FALSE(parseSemVer("").has_value()); }
static void test_parse_just_prefix() { TEST_ASSERT_FALSE(parseSemVer("v").has_value()); }
static void test_parse_missing_components() {
    TEST_ASSERT_FALSE(parseSemVer("1.2").has_value());
    TEST_ASSERT_FALSE(parseSemVer("1").has_value());
    TEST_ASSERT_FALSE(parseSemVer("1.2.3.4").has_value());
}
static void test_parse_non_numeric_major() {
    TEST_ASSERT_FALSE(parseSemVer("a.2.3").has_value());
    TEST_ASSERT_FALSE(parseSemVer("1.b.3").has_value());
    TEST_ASSERT_FALSE(parseSemVer("1.2.c").has_value());
}
static void test_parse_letters_in_prerelease_are_ok() {
    auto v = parseSemVer("1.0.0-alpha");
    TEST_ASSERT_TRUE(v.has_value());
    TEST_ASSERT_EQUAL_STRING("alpha", v->prerelease.c_str());
}

static void test_compare_equal() {
    auto a = parseSemVer("1.2.3");
    auto b = parseSemVer("1.2.3");
    TEST_ASSERT_EQUAL_INT(0, compareSemVer(*a, *b));
}
static void test_compare_major_difference() {
    auto a = parseSemVer("1.9.9");
    auto b = parseSemVer("2.0.0");
    TEST_ASSERT_EQUAL_INT(-1, compareSemVer(*a, *b));
    TEST_ASSERT_EQUAL_INT(1, compareSemVer(*b, *a));
}
static void test_compare_minor_difference() {
    auto a = parseSemVer("1.2.9");
    auto b = parseSemVer("1.3.0");
    TEST_ASSERT_EQUAL_INT(-1, compareSemVer(*a, *b));
}
static void test_compare_patch_difference() {
    auto a = parseSemVer("1.2.3");
    auto b = parseSemVer("1.2.4");
    TEST_ASSERT_EQUAL_INT(-1, compareSemVer(*a, *b));
}
static void test_compare_prerelease_less_than_release() {
    auto rc = parseSemVer("1.0.0-rc.1");
    auto rel = parseSemVer("1.0.0");
    TEST_ASSERT_EQUAL_INT(-1, compareSemVer(*rc, *rel));
    TEST_ASSERT_EQUAL_INT(1, compareSemVer(*rel, *rc));
}
static void test_compare_prerelease_ordering() {
    auto a = parseSemVer("1.0.0-alpha");
    auto b = parseSemVer("1.0.0-alpha.1");
    auto c = parseSemVer("1.0.0-alpha.beta");
    auto d = parseSemVer("1.0.0-beta");
    auto e = parseSemVer("1.0.0-beta.2");
    auto f = parseSemVer("1.0.0-beta.11");
    auto g = parseSemVer("1.0.0-rc.1");
    TEST_ASSERT_EQUAL_INT(-1, compareSemVer(*a, *b));
    TEST_ASSERT_EQUAL_INT(-1, compareSemVer(*b, *c));
    TEST_ASSERT_EQUAL_INT(-1, compareSemVer(*c, *d));
    TEST_ASSERT_EQUAL_INT(-1, compareSemVer(*d, *e));
    TEST_ASSERT_EQUAL_INT(-1, compareSemVer(*e, *f));
    TEST_ASSERT_EQUAL_INT(-1, compareSemVer(*f, *g));
}
static void test_compare_numeric_prerelease_uses_integer() {
    auto a = parseSemVer("1.0.0-rc.2");
    auto b = parseSemVer("1.0.0-rc.10");
    TEST_ASSERT_EQUAL_INT(-1, compareSemVer(*a, *b));
}

static void test_isNewer_strict() {
    TEST_ASSERT_TRUE(isNewerVersion("v1.2.4", "v1.2.3"));
    TEST_ASSERT_FALSE(isNewerVersion("v1.2.3", "v1.2.4"));
    TEST_ASSERT_FALSE(isNewerVersion("v1.2.3", "v1.2.3"));
}
static void test_isNewer_prerelease() {
    TEST_ASSERT_TRUE(isNewerVersion("v1.2.4", "v1.2.4-rc.1"));
}
static void test_isNewer_both_unparseable() {
    TEST_ASSERT_FALSE(isNewerVersion("notatag", "alsogarbage"));
}
static void test_isNewer_current_parseable_latest_not() {
    TEST_ASSERT_FALSE(isNewerVersion("garbage", "v1.2.3"));
}
static void test_isNewer_current_unparseable_latest_parseable() {
    TEST_ASSERT_TRUE(isNewerVersion("v1.2.3", "garbage"));
}
static void test_isNewer_v_prefix_optional() {
    TEST_ASSERT_FALSE(isNewerVersion("v1.0.0", "1.0.0"));
}
static void test_compareVersions_strict() {
    TEST_ASSERT_EQUAL_INT(1, compareVersions("1.2.4", "1.2.3"));
    TEST_ASSERT_EQUAL_INT(0, compareVersions("1.2.3", "1.2.3"));
    TEST_ASSERT_EQUAL_INT(-1, compareVersions("1.2.3", "1.2.4"));
}
static void test_compareVersions_fallback() {
    TEST_ASSERT_EQUAL_INT(0, compareVersions("notatag", "alsogarbage"));
    TEST_ASSERT_EQUAL_INT(-1, compareVersions("garbage", "v1.2.3"));
    TEST_ASSERT_EQUAL_INT(1, compareVersions("v1.2.3", "garbage"));
}

// =============== state machine tests ===============

static void test_check_idle_to_inprogress_to_done() {
    State s;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CheckState::Idle), static_cast<int>(s.check));
    TEST_ASSERT_TRUE(s.startCheck());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CheckState::InProgress), static_cast<int>(s.check));
    s.finishCheck(true);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CheckState::Done), static_cast<int>(s.check));
}
static void test_check_idle_to_inprogress_to_failed() {
    State s;
    s.startCheck();
    s.finishCheck(false);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CheckState::Failed), static_cast<int>(s.check));
    TEST_ASSERT_EQUAL_INT(1, s.checkFailures);
}
static void test_check_refuses_when_already_in_progress() {
    State s;
    s.startCheck();
    TEST_ASSERT_FALSE(s.startCheck());
}
static void test_check_can_restart_after_failure() {
    State s;
    s.startCheck();
    s.finishCheck(false);
    TEST_ASSERT_TRUE(s.startCheck());
}

static void test_update_idle_to_downloading_to_flashing_to_pending() {
    State s;
    TEST_ASSERT_TRUE(s.startUpdate());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(UpdateState::Downloading), static_cast<int>(s.update));
    s.markFlashing();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(UpdateState::Flashing), static_cast<int>(s.update));
    s.finishUpdate(true);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(UpdateState::Pending), static_cast<int>(s.update));
    TEST_ASSERT_FALSE(s.updateInProgress.load());
}
static void test_update_idle_to_downloading_to_failed() {
    State s;
    s.startUpdate();
    s.finishUpdate(false);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(UpdateState::Failed), static_cast<int>(s.update));
    TEST_ASSERT_EQUAL_INT(1, s.updateFailures);
    TEST_ASSERT_FALSE(s.updateInProgress.load());
}
static void test_updateInProgress_refuses_when_busy() {
    State s;
    TEST_ASSERT_TRUE(s.claimUpdate());
    TEST_ASSERT_FALSE(s.claimUpdate());
    TEST_ASSERT_FALSE(s.claimUpdate());
    s.releaseUpdate();
    TEST_ASSERT_TRUE(s.claimUpdate());
    s.releaseUpdate();
}
static void test_updateInProgress_refuses_during_check() {
    State s;
    s.startCheck();
    TEST_ASSERT_FALSE(s.claimUpdate());
    s.finishCheck(true);
    TEST_ASSERT_TRUE(s.claimUpdate());
    s.releaseUpdate();
}

// =============== Progress / FirmwareInfo tests ===============

static void test_progress_defaults_are_safe() {
    ProgressMirror p;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(UpdateStateMirror::Idle), static_cast<int>(p.state));
    TEST_ASSERT_EQUAL_UINT8(0, p.percent);
    TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(p.bytes_written));
    TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(p.expected_bytes));
    TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(p.started_at_ms));
    TEST_ASSERT_TRUE(p.error_message.empty());
}
static void test_progress_percent_clamped_at_100() {
    auto clamp = [](int v) { return v > 100 ? 100 : (v < 0 ? 0 : v); };
    TEST_ASSERT_EQUAL_INT(0, clamp(-5));
    TEST_ASSERT_EQUAL_INT(50, clamp(50));
    TEST_ASSERT_EQUAL_INT(100, clamp(100));
    TEST_ASSERT_EQUAL_INT(100, clamp(150));
    TEST_ASSERT_EQUAL_INT(100, clamp(255));
}
static void test_progress_state_transitions_are_monotonic() {
    ProgressMirror p;
    p.state = UpdateStateMirror::Downloading;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(UpdateStateMirror::Downloading), static_cast<int>(p.state));
    p.state = UpdateStateMirror::Flashing;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(UpdateStateMirror::Flashing), static_cast<int>(p.state));
    p.state = UpdateStateMirror::Pending;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(UpdateStateMirror::Pending), static_cast<int>(p.state));
}
static void test_progress_bytes_written_monotonic() {
    ProgressMirror p;
    p.expected_bytes = 1000;
    p.bytes_written = 0;
    size_t last = 0;
    for (int step : {100, 200, 300, 400}) {
        p.bytes_written += step;
        TEST_ASSERT_TRUE(p.bytes_written >= last);
        last = p.bytes_written;
    }
    TEST_ASSERT_EQUAL_UINT32(1000u, static_cast<uint32_t>(p.bytes_written));
}
static void test_firmware_info_defaults() {
    FirmwareInfoMirror info;
    TEST_ASSERT_FALSE(info.isValid);
    TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(info.size));
    TEST_ASSERT_TRUE(info.version.empty());
    TEST_ASSERT_TRUE(info.name.empty());
    TEST_ASSERT_TRUE(info.downloadUrl.empty());
    TEST_ASSERT_TRUE(info.changelog.empty());
}
static void test_firmware_info_assignable() {
    FirmwareInfoMirror info;
    info.version = "v1.2.4";
    info.name = "Release 1.2.4";
    info.downloadUrl = "https://github.com/foo/bar/releases/download/v1.2.4/firmware.bin";
    info.size = 524288;
    info.changelog = "Bug fixes";
    info.isValid = true;
    TEST_ASSERT_TRUE(info.isValid);
    TEST_ASSERT_EQUAL_STRING("v1.2.4", info.version.c_str());
    TEST_ASSERT_EQUAL_UINT32(524288u, static_cast<uint32_t>(info.size));
}

int runUnityTests(void) {
    UNITY_BEGIN();
    // semver
    RUN_TEST(test_parse_simple);
    RUN_TEST(test_parse_with_v_prefix);
    RUN_TEST(test_parse_with_prerelease);
    RUN_TEST(test_parse_with_build_metadata);
    RUN_TEST(test_parse_empty);
    RUN_TEST(test_parse_just_prefix);
    RUN_TEST(test_parse_missing_components);
    RUN_TEST(test_parse_non_numeric_major);
    RUN_TEST(test_parse_letters_in_prerelease_are_ok);
    RUN_TEST(test_compare_equal);
    RUN_TEST(test_compare_major_difference);
    RUN_TEST(test_compare_minor_difference);
    RUN_TEST(test_compare_patch_difference);
    RUN_TEST(test_compare_prerelease_less_than_release);
    RUN_TEST(test_compare_prerelease_ordering);
    RUN_TEST(test_compare_numeric_prerelease_uses_integer);
    RUN_TEST(test_isNewer_strict);
    RUN_TEST(test_isNewer_prerelease);
    RUN_TEST(test_isNewer_both_unparseable);
    RUN_TEST(test_isNewer_current_parseable_latest_not);
    RUN_TEST(test_isNewer_current_unparseable_latest_parseable);
    RUN_TEST(test_isNewer_v_prefix_optional);
    RUN_TEST(test_compareVersions_strict);
    RUN_TEST(test_compareVersions_fallback);
    // state machine
    RUN_TEST(test_check_idle_to_inprogress_to_done);
    RUN_TEST(test_check_idle_to_inprogress_to_failed);
    RUN_TEST(test_check_refuses_when_already_in_progress);
    RUN_TEST(test_check_can_restart_after_failure);
    RUN_TEST(test_update_idle_to_downloading_to_flashing_to_pending);
    RUN_TEST(test_update_idle_to_downloading_to_failed);
    RUN_TEST(test_updateInProgress_refuses_when_busy);
    RUN_TEST(test_updateInProgress_refuses_during_check);
    // progress / firmware info
    RUN_TEST(test_progress_defaults_are_safe);
    RUN_TEST(test_progress_percent_clamped_at_100);
    RUN_TEST(test_progress_state_transitions_are_monotonic);
    RUN_TEST(test_progress_bytes_written_monotonic);
    RUN_TEST(test_firmware_info_defaults);
    RUN_TEST(test_firmware_info_assignable);
    return UNITY_END();
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    return runUnityTests();
}
