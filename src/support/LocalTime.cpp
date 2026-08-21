#include "LocalTime.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace LocalTime {
    namespace {
        constexpr size_t TZ_MAX_LEN = 63;
        constexpr const char *TZ_FALLBACK = "UTC0";

        /**
         * Point libc at the requested zone.
         *
         * setenv() + tzset() re-parses the string and rebuilds libc's
         * timezone state, so it is skipped when the zone is already the one
         * in effect. The comparison reads the environment rather than a
         * private copy of the last-applied string: anything else calling
         * setenv("TZ", …) — configTime() does, at boot — would desync a
         * private copy and leave us reading the wrong zone.
         *
         * tzset() is called explicitly rather than relying on localtime_r:
         * POSIX permits localtime_r not to call it.
         */
        void applyTz(const char *tz) {
            if (tz == nullptr || tz[0] == '\0') {
                tz = TZ_FALLBACK;
            }

            const char *current = getenv("TZ");
            if (current != nullptr && strcmp(current, tz) == 0) {
                return;
            }

            setenv("TZ", tz, 1);
            tzset();
        }

        struct tm localParts(uint32_t epoch, const char *tz) {
            applyTz(tz);
            const time_t when = static_cast<time_t>(epoch);
            struct tm parts = {};
            localtime_r(&when, &parts);
            return parts;
        }
    }

    uint32_t secondsSinceMidnight(uint32_t epoch, const char *tz) {
        if (epoch == 0) return 0;

        const struct tm parts = localParts(epoch, tz);
        return static_cast<uint32_t>(parts.tm_hour) * 3600u +
               static_cast<uint32_t>(parts.tm_min) * 60u +
               static_cast<uint32_t>(parts.tm_sec);
    }

    uint16_t localDayOfYear(uint32_t epoch, const char *tz) {
        const struct tm parts = localParts(epoch, tz);
        return static_cast<uint16_t>(parts.tm_yday);
    }

    Info describe(uint32_t epoch, const char *tz) {
        const struct tm local = localParts(epoch, tz);

        // tm_gmtoff and tm_zone are BSD/GNU extensions, and the ESP32's
        // newlib compiles struct tm without either. Both are derived here
        // instead, from fields the C standard guarantees.
        const time_t when = static_cast<time_t>(epoch);
        struct tm utc = {};
        gmtime_r(&when, &utc);

        int dayDelta = local.tm_yday - utc.tm_yday;
        if (local.tm_year != utc.tm_year) {
            // Across a new year the yday difference is meaningless; the two
            // can only ever be one day apart.
            dayDelta = local.tm_year > utc.tm_year ? 1 : -1;
        }

        Info info = {};
        info.offset_minutes = static_cast<int16_t>(dayDelta * 1440 +
                                                   (local.tm_hour - utc.tm_hour) * 60 +
                                                   (local.tm_min - utc.tm_min));
        info.is_dst = local.tm_isdst > 0;

        // %Z resolves to the designator tzset() parsed out of the TZ string.
        if (strftime(info.abbrev, sizeof(info.abbrev), "%Z", &local) == 0) {
            info.abbrev[0] = '\0';
        }

        return info;
    }

    void legacyOffsetToPosix(int8_t hours, char *out, size_t len) {
        if (out == nullptr || len == 0) return;

        // The negation is the point: POSIX counts west of UTC, so an old
        // stored +1 (an hour east) becomes "UTC-1".
        snprintf(out, len, "UTC%+d", -static_cast<int>(hours));
    }

    bool isSyntacticallyValidTz(const char *tz) {
        if (tz == nullptr) return false;

        const size_t length = strnlen(tz, TZ_MAX_LEN + 1);
        if (length == 0 || length > TZ_MAX_LEN) return false;

        for (size_t i = 0; i < length; i++) {
            const unsigned char c = static_cast<unsigned char>(tz[i]);
            if (c < 0x20 || c > 0x7E) return false; // printable ASCII only
            if (c == '=') return false;             // would corrupt the environment entry
        }

        // A zone designator is either three or more letters, or a bracketed
        // form such as <+0545>. Everything else is not a TZ string.
        if (tz[0] == '<') {
            return strchr(tz, '>') != nullptr;
        }

        for (size_t i = 0; i < 3; i++) {
            const unsigned char c = static_cast<unsigned char>(tz[i]);
            const bool alphabetic = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
            if (!alphabetic) return false;
        }

        return true;
    }
}
