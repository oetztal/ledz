#include "unity.h"
#include "support/LocalTime.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

void setUp() {}

void tearDown() {}

namespace {
    constexpr const char *BERLIN = "CET-1CEST,M3.5.0,M10.5.0/3";

    /**
     * UTC epoch for a calendar instant. timegm() is the inverse of gmtime()
     * and, unlike mktime(), does not consult TZ — which matters here, since
     * every call under test rewrites it.
     */
    uint32_t utcEpoch(int year, int month, int day, int hour, int minute, int second) {
        struct tm parts = {};
        parts.tm_year = year - 1900;
        parts.tm_mon = month - 1;
        parts.tm_mday = day;
        parts.tm_hour = hour;
        parts.tm_min = minute;
        parts.tm_sec = second;
        return static_cast<uint32_t>(timegm(&parts));
    }

    uint32_t hms(int hour, int minute, int second) {
        return static_cast<uint32_t>(hour) * 3600u + static_cast<uint32_t>(minute) * 60u +
               static_cast<uint32_t>(second);
    }

    /** Offset in minutes east of UTC that the host's own tzdata reports. */
    int hostOffsetMinutes(const char *zone, uint32_t epoch) {
        setenv("TZ", zone, 1);
        tzset();
        const time_t when = static_cast<time_t>(epoch);
        struct tm parts = {};
        localtime_r(&when, &parts);
        return static_cast<int>(parts.tm_gmtoff / 60);
    }

    struct TableRow {
        std::string tz;
        std::string iana;
    };

    /**
     * The dropdown rows as they ship, parsed straight out of the page. The
     * <option> value *is* the POSIX string the device will store, so this
     * tests the artifact rather than a copy of it.
     */
    std::vector<TableRow> readTimezoneTable() {
        static const char *candidates[] = {
            "data/timers.html",
            "../data/timers.html",
            "../../data/timers.html",
            "../../../data/timers.html",
        };

        FILE *page = nullptr;
        for (const char *path : candidates) {
            page = fopen(path, "r");
            if (page != nullptr) break;
        }
        TEST_ASSERT_NOT_NULL_MESSAGE(page, "could not locate data/timers.html");

        std::vector<TableRow> rows;
        char line[512];
        while (fgets(line, sizeof(line), page) != nullptr) {
            const char *value = strstr(line, "<option value=\"");
            if (value == nullptr) continue;
            value += strlen("<option value=\"");
            const char *valueEnd = strchr(value, '"');
            if (valueEnd == nullptr) continue;

            const char *iana = strstr(valueEnd, "data-iana=\"");
            if (iana == nullptr) continue;
            iana += strlen("data-iana=\"");
            const char *ianaEnd = strchr(iana, '"');
            if (ianaEnd == nullptr) continue;

            // data-iana carries the canonical zone first, then aliases.
            const char *canonicalEnd = static_cast<const char *>(memchr(iana, ' ', ianaEnd - iana));
            if (canonicalEnd == nullptr) canonicalEnd = ianaEnd;

            rows.push_back({std::string(value, valueEnd - value),
                            std::string(iana, canonicalEnd - iana)});
        }
        fclose(page);
        return rows;
    }
}

// --- secondsSinceMidnight -------------------------------------------------

void test_berlin_winter_and_summer() {
    TEST_ASSERT_EQUAL_UINT32(hms(7, 0, 0),
                             LocalTime::secondsSinceMidnight(utcEpoch(2026, 1, 15, 6, 0, 0), BERLIN));
    TEST_ASSERT_EQUAL_UINT32(hms(7, 0, 0),
                             LocalTime::secondsSinceMidnight(utcEpoch(2026, 7, 15, 5, 0, 0), BERLIN));
}

void test_berlin_spring_forward_instant() {
    const uint32_t beforeGap = utcEpoch(2026, 3, 29, 0, 59, 59);
    TEST_ASSERT_EQUAL_UINT32(hms(1, 59, 59), LocalTime::secondsSinceMidnight(beforeGap, BERLIN));
    TEST_ASSERT_EQUAL_UINT32(hms(3, 0, 0), LocalTime::secondsSinceMidnight(beforeGap + 1, BERLIN));
}

void test_sub_hour_offset() {
    TEST_ASSERT_EQUAL_UINT32(hms(17, 30, 0),
                             LocalTime::secondsSinceMidnight(utcEpoch(2026, 6, 1, 12, 0, 0), "IST-5:30"));
}

void test_zone_without_dst_is_constant_all_year() {
    for (int month = 1; month <= 12; month++) {
        TEST_ASSERT_EQUAL_UINT32(hms(12, 0, 0),
                                 LocalTime::secondsSinceMidnight(utcEpoch(2026, month, 15, 15, 0, 0), "<-03>3"));
    }
}

void test_epoch_zero_is_midnight() {
    TEST_ASSERT_EQUAL_UINT32(0, LocalTime::secondsSinceMidnight(0, BERLIN));
}

// --- legacyOffsetToPosix --------------------------------------------------

void test_legacy_offset_round_trips_for_every_hour() {
    // Guards the POSIX sign inversion: an old +1 must come out as UTC-1,
    // which is still an hour east of UTC.
    const uint32_t epoch = utcEpoch(2026, 7, 15, 12, 0, 0);

    for (int hours = -12; hours <= 14; hours++) {
        char tz[16];
        LocalTime::legacyOffsetToPosix(static_cast<int8_t>(hours), tz, sizeof(tz));

        char expected[16];
        snprintf(expected, sizeof(expected), "UTC%+d", -hours);
        TEST_ASSERT_EQUAL_STRING(expected, tz);

        const LocalTime::Info info = LocalTime::describe(epoch, tz);
        TEST_ASSERT_EQUAL_INT(hours * 60, info.offset_minutes);
        TEST_ASSERT_FALSE(info.is_dst);
    }
}

// --- the shipped zone table ----------------------------------------------

void test_table_offsets_match_host_tzdata() {
    const std::vector<TableRow> rows = readTimezoneTable();
    TEST_ASSERT_GREATER_THAN_UINT32(20, static_cast<uint32_t>(rows.size()));

    const uint32_t probes[] = {
        utcEpoch(2026, 1, 15, 12, 0, 0),
        utcEpoch(2026, 7, 15, 12, 0, 0),
    };

    for (const TableRow &row : rows) {
        for (uint32_t epoch : probes) {
            const int expected = hostOffsetMinutes(row.iana.c_str(), epoch);
            const LocalTime::Info info = LocalTime::describe(epoch, row.tz.c_str());

            char message[160];
            snprintf(message, sizeof(message), "%s (%s): expected %d, got %d",
                     row.iana.c_str(), row.tz.c_str(), expected, info.offset_minutes);
            TEST_ASSERT_EQUAL_INT_MESSAGE(expected, info.offset_minutes, message);
        }
    }
}

// --- localDayOfYear -------------------------------------------------------

void test_day_of_year_is_shared_by_both_fall_back_instants() {
    // 2026-10-25: Berlin runs 02:00-02:59 CEST, falls back, runs it again CET.
    const uint32_t firstHalfPast = utcEpoch(2026, 10, 25, 0, 30, 0);  // 02:30 CEST
    const uint32_t secondHalfPast = utcEpoch(2026, 10, 25, 1, 30, 0); // 02:30 CET

    TEST_ASSERT_EQUAL_UINT32(hms(2, 30, 0), LocalTime::secondsSinceMidnight(firstHalfPast, BERLIN));
    TEST_ASSERT_EQUAL_UINT32(hms(2, 30, 0), LocalTime::secondsSinceMidnight(secondHalfPast, BERLIN));

    TEST_ASSERT_EQUAL_UINT16(LocalTime::localDayOfYear(firstHalfPast, BERLIN),
                             LocalTime::localDayOfYear(secondHalfPast, BERLIN));
}

void test_day_of_year_changes_across_local_midnight() {
    const uint32_t beforeMidnight = utcEpoch(2026, 6, 14, 21, 59, 0); // 23:59 CEST
    const uint32_t afterMidnight = utcEpoch(2026, 6, 14, 22, 1, 0);   // 00:01 CEST next day

    TEST_ASSERT_NOT_EQUAL(LocalTime::localDayOfYear(beforeMidnight, BERLIN),
                          LocalTime::localDayOfYear(afterMidnight, BERLIN));
}

void test_day_of_year_is_local_not_utc() {
    // 00:30 local on Jan 2nd is still Jan 1st in UTC.
    const uint32_t epoch = utcEpoch(2026, 1, 1, 23, 30, 0);
    TEST_ASSERT_EQUAL_UINT16(1, LocalTime::localDayOfYear(epoch, BERLIN));
    TEST_ASSERT_EQUAL_UINT16(0, LocalTime::localDayOfYear(epoch, "UTC0"));
}

// --- describe -------------------------------------------------------------

void test_describe_reports_dst_state_and_abbreviation() {
    const LocalTime::Info winter = LocalTime::describe(utcEpoch(2026, 1, 15, 12, 0, 0), BERLIN);
    TEST_ASSERT_EQUAL_INT(60, winter.offset_minutes);
    TEST_ASSERT_FALSE(winter.is_dst);
    TEST_ASSERT_EQUAL_STRING("CET", winter.abbrev);

    const LocalTime::Info summer = LocalTime::describe(utcEpoch(2026, 7, 15, 12, 0, 0), BERLIN);
    TEST_ASSERT_EQUAL_INT(120, summer.offset_minutes);
    TEST_ASSERT_TRUE(summer.is_dst);
    TEST_ASSERT_EQUAL_STRING("CEST", summer.abbrev);
}

void test_describe_truncates_a_long_abbreviation() {
    const LocalTime::Info info = LocalTime::describe(utcEpoch(2026, 7, 15, 12, 0, 0), "<+0545>-5:45");
    TEST_ASSERT_EQUAL_INT(345, info.offset_minutes);
    TEST_ASSERT_TRUE(strlen(info.abbrev) < sizeof(info.abbrev));
}

// --- isSyntacticallyValidTz ----------------------------------------------

void test_valid_tz_strings_are_accepted() {
    TEST_ASSERT_TRUE(LocalTime::isSyntacticallyValidTz("UTC0"));
    TEST_ASSERT_TRUE(LocalTime::isSyntacticallyValidTz("IST-5:30"));
    TEST_ASSERT_TRUE(LocalTime::isSyntacticallyValidTz("<+0545>-5:45"));
    TEST_ASSERT_TRUE(LocalTime::isSyntacticallyValidTz(BERLIN));
    TEST_ASSERT_TRUE(LocalTime::isSyntacticallyValidTz("<+1245>-12:45<+1345>,M9.5.0/2:45,M4.1.0/3:45"));
}

void test_invalid_tz_strings_are_rejected() {
    TEST_ASSERT_FALSE(LocalTime::isSyntacticallyValidTz(nullptr));
    TEST_ASSERT_FALSE(LocalTime::isSyntacticallyValidTz(""));
    TEST_ASSERT_FALSE(LocalTime::isSyntacticallyValidTz("CET-1=CEST"));
    TEST_ASSERT_FALSE(LocalTime::isSyntacticallyValidTz("CET\t-1"));
    TEST_ASSERT_FALSE(LocalTime::isSyntacticallyValidTz("12"));
    TEST_ASSERT_FALSE(LocalTime::isSyntacticallyValidTz("C1T-1"));
    TEST_ASSERT_FALSE(LocalTime::isSyntacticallyValidTz("<+0545-5:45"));

    char tooLong[80];
    memset(tooLong, 'A', sizeof(tooLong));
    tooLong[64] = '\0';
    TEST_ASSERT_FALSE(LocalTime::isSyntacticallyValidTz(tooLong));
    tooLong[63] = '\0';
    TEST_ASSERT_TRUE(LocalTime::isSyntacticallyValidTz(tooLong));
}

int main(int, char **) {
    UNITY_BEGIN();

    RUN_TEST(test_berlin_winter_and_summer);
    RUN_TEST(test_berlin_spring_forward_instant);
    RUN_TEST(test_sub_hour_offset);
    RUN_TEST(test_zone_without_dst_is_constant_all_year);
    RUN_TEST(test_epoch_zero_is_midnight);

    RUN_TEST(test_legacy_offset_round_trips_for_every_hour);

    RUN_TEST(test_table_offsets_match_host_tzdata);

    RUN_TEST(test_day_of_year_is_shared_by_both_fall_back_instants);
    RUN_TEST(test_day_of_year_changes_across_local_midnight);
    RUN_TEST(test_day_of_year_is_local_not_utc);

    RUN_TEST(test_describe_reports_dst_state_and_abbreviation);
    RUN_TEST(test_describe_truncates_a_long_abbreviation);

    RUN_TEST(test_valid_tz_strings_are_accepted);
    RUN_TEST(test_invalid_tz_strings_are_rejected);

    return UNITY_END();
}
