//
// Local-time conversion from a POSIX TZ string
//
// The device stores its timezone as a POSIX TZ string
// ("CET-1CEST,M3.5.0,M10.5.0/3") and lets the C library do the DST
// arithmetic. Note that the POSIX offset is *west* of UTC, so the sign
// reads backwards: CET-1CEST is UTC+1 and EST5EDT is UTC-5.
//
// ⚠ These functions mutate process-global state. Each one applies the
// requested zone with setenv("TZ", …) + tzset() before calling
// localtime_r(), because that is the only way to ask libc for a specific
// zone. Callers must therefore confine themselves to a single task: on the
// device that is the Network task, which owns checkTimers(). A request
// handler running on the AsyncTCP task must hand the string over via a
// dirty flag rather than call these directly, or it races a live alarm
// evaluation.
//

#ifndef LEDZ_LOCAL_TIME_H
#define LEDZ_LOCAL_TIME_H

#include <cstddef>
#include <cstdint>

namespace LocalTime {
    /**
     * The zone as it stands at one instant.
     */
    struct Info {
        int16_t offset_minutes; // minutes east of UTC, so +120 for CEST
        bool is_dst;            // true while the daylight-saving rule is in effect
        char abbrev[8];         // "CEST", "IST", "+0545"
    };

    /**
     * Seconds elapsed since local midnight.
     * @param epoch UTC epoch seconds
     * @param tz POSIX TZ string
     * @return 0..86399, or 0 if epoch is 0
     */
    uint32_t secondsSinceMidnight(uint32_t epoch, const char *tz);

    /**
     * Local day of the year, used to fire a daily alarm at most once per
     * local day. Both 02:30 instants of a fall-back night share a value.
     * @param epoch UTC epoch seconds
     * @param tz POSIX TZ string
     * @return 0..365
     */
    uint16_t localDayOfYear(uint32_t epoch, const char *tz);

    /**
     * Describe the zone as it stands at the given instant.
     * @param epoch UTC epoch seconds
     * @param tz POSIX TZ string
     * @return Offset, DST state and abbreviation
     */
    Info describe(uint32_t epoch, const char *tz);

    /**
     * Convert a legacy whole-hour UTC offset to an equivalent fixed-offset
     * POSIX string, preserving the old firmware's timekeeping exactly.
     * The sign flips: +1 becomes "UTC-1", which is UTC+1.
     * @param hours Legacy offset in hours east of UTC (-12..+14)
     * @param out Destination buffer
     * @param len Size of the destination buffer
     */
    void legacyOffsetToPosix(int8_t hours, char *out, size_t len);

    /**
     * Syntactic plausibility check for a POSIX TZ string.
     *
     * Necessarily syntactic only: a tzset() round-trip would be the same
     * global mutation the single-task rule exists to avoid, and newlib
     * silently falls back to UTC on an unparseable string rather than
     * reporting an error, so a round-trip could not reject it anyway.
     *
     * @param tz Candidate string
     * @return true if it is worth storing
     */
    bool isSyntacticallyValidTz(const char *tz);
}

#endif //LEDZ_LOCAL_TIME_H
