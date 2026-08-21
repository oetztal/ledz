## Context

Time enters the device in exactly one place: `NTPClient` on the `Network` task (`Network.cpp:351-362`), which polls every 600 s and hands a UTC epoch to `TimerScheduler::checkTimers()` once per loop iteration. Everything downstream is derived:

```
                 ┌──────────────────────────────────────────────────────────┐
   pool.ntp.org  │  Network task (core 0, ~1 Hz)                            │
        │        │                                                          │
        └───────▶│  ntpClient.getEpochTime()  ──▶ checkTimers(epoch)        │
                 │                                    │                     │
                 │                                    ├─ COUNTDOWN:         │
                 │                                    │    epoch >= target  │  DST-immune
                 │                                    │                     │
                 │                                    └─ ALARM_DAILY:       │
                 │                                         getSecondsSince- │  DST-sensitive
                 │                                         Midnight(epoch)  │
                 └──────────────────────────────────────────────────────────┘
                                       ▲
                 ┌─────────────────────┴────────────────────────────────────┐
                 │  AsyncTCP task — GET /api/timers, POST /api/timers/tz    │
                 └──────────────────────────────────────────────────────────┘
```

`Network.cpp:208` calls `configTime(0, 0, …)` deliberately: lwip's SNTP calls `settimeofday()`, which is what mbedtls reads during the OTA TLS handshake. That call also sets the process `TZ` to UTC — which is why the current code hand-rolls the offset instead of using `localtime()`.

The relevant state today:

```
  ┌──────────────────────────┬──────────┬────────────────┬──────────────────────────────┐
  │ field                    │ type     │ NVS key        │ meaning                      │
  ├──────────────────────────┼──────────┼────────────────┼──────────────────────────────┤
  │ timezone_offset_hours    │ int8_t   │ tz_offset      │ hours east of UTC, -12..+14  │
  │ TimerEntry.target_time   │ uint32_t │ timer_N_target │ epoch │ secs-since-midnight  │
  │ TimerEntry.duration_secs │ uint32_t │ timer_N_dur    │ duration │ last-fired minute │
  └──────────────────────────┴──────────┴────────────────┴──────────────────────────────┘
```

Both of the last two fields are unions-by-convention discriminated on `type`. `duration_seconds` is the problem one: the header calls it "original duration for countdown display", but `TimerScheduler.cpp:86` overwrites it with `currentEpoch / 60` for daily alarms.

There is no NVS schema version anywhere in `Config.cpp`, and no migration has ever been performed.

## Goals / Non-Goals

**Goals:**
- A daily alarm set for 07:00 fires at 07:00 local, year-round, without user intervention
- Sub-hour offsets are expressible
- The user picks a *place*, not an offset — and can have the browser guess it
- Exactly one alarm firing per local day, across both transitions
- The DST rules the device uses are inspectable and overridable without a firmware build
- The local-time conversion is unit-tested on the host

**Non-Goals:**
- On-device tzdata, online lookups, or automatic rule updates
- Catch-up firing for alarms skipped by the spring-forward gap
- A general NVS schema-version mechanism
- Any change to countdown-timer behaviour

## Decisions

### 1. A POSIX TZ string is the stored representation

Three candidates:

| | storage | DST | sub-hour | firmware cost | rule updates |
|---|---|---|---|---|---|
| offset in minutes + `dst_rule` enum | 3 B | only the rules we enumerate | yes | rule table + evaluator | rebuild |
| **POSIX TZ string** | **64 B** | **yes, any rule** | **yes** | **~0 (newlib)** | **user-editable** |
| IANA name + on-device tzdata | ~500 KB | yes | yes | very large | tzdata bundle |

newlib's `tzset()` parses the full POSIX grammar — `std offset dst [offset],start[/time],end[/time]` — and `localtime_r()` applies it. The parser and the transition arithmetic are already in the binary because `localtime_r` is already linked. The marginal cost of correct DST is therefore a 64-byte NVS string and deleting the hand-rolled arithmetic at `TimerScheduler.cpp:36`.

⚠️ **The sign is inverted.** In POSIX, the offset is *west* of UTC, so `CET-1CEST` means UTC**+**1 and `EST5EDT` means UTC**−**5. This is the single most likely bug in the change; decision 9 exists mostly to test it.

64 bytes is sized off the worst realistic entry — Chatham's `<+1245>-12:45<+1345>,M9.5.0/2:45,M4.1.0/3:45` is 44 characters.

### 2. The zone table lives in `data/timers.html`, not in firmware

If the `<option>` value *is* the POSIX string, the device never needs a table at all:

```html
<option value="CET-1CEST,M3.5.0,M10.5.0/3" data-iana="Europe/Berlin">Berlin, Paris, Madrid, Rome</option>
<option value="IST-5:30"                   data-iana="Asia/Kolkata">India</option>
<option value="EST5EDT,M3.2.0,M11.1.0"     data-iana="America/New_York">New York, Toronto</option>
```

The device stores the string, returns the string, and `select.value = data.timezone` re-selects the matching row for free. No `/api/timezones` endpoint, no `PROGMEM` table, no label/value mapping to keep in sync across two languages. The cost is ~40 rows × ~70 bytes ≈ 2.8 KB of HTML, which gzips well and replaces the 25 rows already there.

Rejected: a firmware table served over HTTP. It puts the same data behind an extra request, grows the binary, and gains nothing — the firmware has no use for a human label.

The table is **generated, not hand-typed**. The last line of a TZif v2+ file is its POSIX footer:

```
$ awk 'END{print}' /usr/share/zoneinfo/Europe/Berlin
CET-1CEST,M3.5.0,M10.5.0/3
```

A script reads that for each chosen IANA zone, so the strings are sourced from tzdata rather than from memory. This matters: several are counterintuitive (Brazil abolished DST in 2019 → `<-03>3`; Mexico in 2022 → `CST6`; Iran in 2022 → `<+0330>-3:30`; Egypt reinstated it in 2023).

### 3. Detect selects, it does not save

```
[Detect]  ──► Intl.DateTimeFormat().resolvedOptions().timeZone
                   │  "Europe/Berlin"
                   ├─ exact data-iana match ──────► select the option, flag it visually
                   ├─ no match, but some option's current offset AND
                   │  six-months-from-now offset both agree ────► select that one
                   └─ still nothing ──────────────► leave the selection alone,
                                                    show "couldn't match your zone —
                                                    please pick it manually"
```

The two-probe fallback (now and +6 months) distinguishes a fixed-offset zone from a DST one without needing the transition dates, which is enough to pick a sensible row for an unlisted zone.

Detect never issues the POST. A button that silently rewrites device configuration on click is a surprise; `Save Timezone` stays the only committing action, matching how every other control on the page behaves.

### 4. `TZ` is applied on the Network task only

`setenv("TZ", …)` + `tzset()` mutates process-global libc state, and `localtime_r()` reads it. The POST handler runs on the AsyncTCP task while `checkTimers()` runs on the Network task — applying TZ directly from the handler is a data race against a live alarm evaluation.

```
  AsyncTCP task                      Network task (~1 Hz)
  ─────────────                      ─────────────────────
  POST /api/timers/timezone
    │
    ├─ syntactic validation ──400 on failure
    ├─ scheduler->setTimezone(str)
    │     ├─ copy into timersConfig.timezone
    │     ├─ saveTimersConfig()
    │     └─ tzDirty = true   ────────────▶  checkTimers():
    └─ 200 {"success":true}                    if (tzDirty) { setenv; tzset; tzDirty=false;
                                                              log abbrev + offset }
                                               … evaluate alarms …
```

Worst-case apply latency is one loop iteration (<1 s), so the UI simply re-fetches `/api/timers` after the POST resolves and reads the already-updated `tz_abbrev`.

Validation is necessarily **syntactic** — a `tzset()` round-trip would be the same global mutation this decision exists to avoid, and newlib silently falls back to UTC on an unparseable string rather than reporting an error, so a round-trip could not reject it anyway. The gate is therefore: non-empty, ≤ 63 chars, printable ASCII, no `=`, and at least three leading characters that are letters or a `<…>` designator. The Network task logs the resulting abbreviation and offset after applying, which is how a bad-but-syntactically-valid string gets diagnosed.

`configTime(0, 0, …)` at `Network.cpp:208` sets `TZ` to UTC as a side effect, so it must run *before* the first apply. Keeping the apply inside `checkTimers` gets this ordering for free — `checkTimers` is only ever reached after `configTime`.

### 5. Legacy offsets migrate to a fixed offset, not a guessed region

An old stored `+1` is genuinely ambiguous: it could be Berlin (DST) or Lagos (no DST). Two options:

- **Guess the region** — map `+1` → `CET-1CEST,…`. Correct for most users, but it *silently changes when their lights turn on*, for users who never asked for anything. A device that starts behaving differently after an OTA is worse than one that keeps its old behaviour.
- **Preserve behaviour** — map `+1` → `UTC-1`, a fixed UTC+1 with no DST, which is exactly what the device did before.

Behaviour-preserving wins, with the gap closed in the UI rather than silently: a stored value matching `^UTC[+-]?\d+$` fails to match any dropdown row, and that miss triggers a banner on the timers page — *"Your timezone is a fixed UTC offset and will not follow daylight saving. Pick your region below to enable it."* The same "unmatched value" path also serves advanced raw strings, which get shown in the advanced field instead of the banner.

Note the POSIX negation in the mapping: `snprintf(buf, "UTC%+d", -old_offset)`. Old `+1` → `UTC-1`. Old `-5` → `UTC+5`.

### 6. Migration is on-read with write-back, keyed on key presence

No schema version exists in `Config.cpp` today, and introducing one for a single field would mean auditing and versioning every other key in the namespace — a much larger change with its own risk. Key presence is sufficient here because the new and old keys are disjoint:

```
loadTimersConfig():
    if (prefs.isKey("tz")):
        getString("tz", timezone, 64)                    ── already migrated
    else:
        int8_t old = prefs.getChar("tz_offset", 0)
        legacyOffsetToPosix(old) -> timezone             ── decision 5
        migrated = true

    for each timer i:
        …
        if (migrated && timers[i].type == ALARM_DAILY):
            timers[i].duration_seconds = 0               ── was a trigger-minute marker
        timers[i].last_fired_yday = prefs.getUShort("timer_N_lfd", NEVER)

    if (migrated):
        saveTimersConfig(timersConfig)   // writes "tz" and the "_lfd" keys
        prefs.remove("tz_offset")
```

Three properties this needs and has:

- **Idempotent.** A second boot sees `tz` present and takes the fast path.
- **Crash-safe.** Power loss before the write-back leaves `tz_offset` in place and re-runs the migration next boot. Power loss *after* `putString("tz")` but before `remove("tz_offset")` leaves a harmless orphan that the next boot ignores and re-removes.
- **Correct across a downgrade.** Rolling back to older firmware after migration finds no `tz_offset` and defaults to `0` — UTC. The user's timezone is lost, not corrupted, and the old firmware still works. Worth stating in the release notes; the OTA rollback path (`OTAUpdater`) makes this reachable.

The `ALARM_DAILY` reset of `duration_seconds` is the part that is easy to miss. Existing devices have a trigger-minute (a number around 29 million) stored where the duration belongs. Left alone it is only cosmetic — `getRemainingSeconds` does not read it for alarms — but it would be a landmine for anything that later trusts the field's documented meaning.

`prefs.remove()` on a missing key is a no-op, and NVS keys are capped at 15 characters: `timer_0_lfd` is 11, and the existing longest, `timer_0_enabled`, is exactly 15.

### 7. Alarm dedup keys on the local day, not the epoch minute

Current marker (`TimerScheduler.cpp:84-90`):

```cpp
uint32_t triggerMinute = currentEpoch / 60;
if (timer.duration_seconds != triggerMinute) { … }
```

This is an *absolute* key guarding a *wall-clock* condition, which is exactly backwards. On a fall-back night the 02:30 window is entered twice with two different epoch minutes, so the guard passes twice:

```
   epoch minute   local wall clock   window 02:30..02:30:05   marker   fires?
   ───────────    ────────────────   ─────────────────────    ──────   ──────
   29,383,470        02:30 CEST              hit              differs    YES
   29,383,530        02:30 CET               hit              differs    YES  ← bug
```

Replacing it with `last_fired_yday` — `tm_yday` of the local day, from the same `localtime_r` call that produced the seconds-since-midnight — makes the key match the condition's frame. Both 02:30s fall on the same local day, so the second is suppressed.

`uint16_t` with `0xFFFF` as "never". Day-of-year rather than a full date because it only has to distinguish *today* from *not today*, and a 366-day wraparound cannot alias with a 1 Hz check.

This also un-overloads `duration_seconds`, which reverts to meaning only "original duration for countdown display" as its comment always claimed.

### 8. The spring-forward gap is skipped, and documented

An alarm at 02:30 in a zone that jumps 02:00 → 03:00 has no instant on that day. Options:

- **Catch up** — fire at the first tick after the gap. Needs the check to detect that the target second was jumped over, which means tracking the previous iteration's local seconds and handling the 1 Hz loop missing a tick for unrelated reasons. Real complexity.
- **Skip** — the existing `>= target && < target + 5` window simply never matches; the alarm does not fire that day and resumes the next.

Skipping. One alarm, one day a year, in the 01:00–03:00 local window, on a device whose purpose is decorative lighting. The behaviour is specified explicitly rather than left as an accident, so it is a documented property rather than a latent surprise.

Symmetrically, fall-back means a 02:30 alarm fires at the *first* 02:30 (still in DST) and is suppressed at the second — decision 7 — so the interval between firings is 23 or 25 hours around the transitions. Also specified.

### 9. The conversion logic lives in `src/support/`

`platformio.ini:14` already includes `+<support/>` in the native build filter, while `TimerScheduler.cpp` and `Config.cpp` are excluded (`Config.cpp` is Arduino-only by construction — see the comment at `Config.h:179-182`). Putting the pure functions in `src/support/LocalTime.{h,cpp}`, alongside the existing `Gamma`, makes them testable on the host with no new build plumbing:

```cpp
namespace LocalTime {
    struct Info { int16_t offset_minutes; bool is_dst; char abbrev[8]; };

    uint32_t secondsSinceMidnight(uint32_t epoch, const char* tz);
    uint16_t localDayOfYear     (uint32_t epoch, const char* tz);
    Info     describe           (uint32_t epoch, const char* tz);
    void     legacyOffsetToPosix(int8_t hours, char* out, size_t len);
    bool     isSyntacticallyValidTz(const char* tz);
}
```

Host libc parses POSIX TZ strings the same way newlib does, so `2026-03-29T00:59:59Z` → `01:59:59` and `+1s` → `03:00:00` for Berlin is a real assertion on the host.

Two caveats the implementation has to respect:

- **`TZ` is process-global.** These functions apply it via `setenv`/`tzset`, cached against the last-applied string so the common case is a pointer compare. That is a hidden global write, and it is why decision 4 confines the calls to one task. Tests must not assume `TZ` is unchanged across a call.
- **`localtime_r` is not required to call `tzset`.** POSIX explicitly permits it not to. The helper calls `tzset()` itself after any `setenv`.

## Risks / Trade-offs

- **The POSIX sign inversion.** Mitigated by generating the table from tzdata (decision 2) rather than hand-typing it, and by a native test asserting the effective offset of every row in the table at two dates six months apart.
- **Baked-in DST rules.** If a jurisdiction changes its rule, affected users get wrong times until an OTA. Mitigated by the advanced raw field, which lets a user fix it without a build. Accepted: this is the price of not shipping tzdata.
- **A bad advanced string silently means UTC.** newlib does not report parse failures. Mitigated by logging the resulting abbreviation and offset after apply (decision 4) and by surfacing `tz_abbrev`/`tz_offset_minutes` in the UI header — a user who typed nonsense sees `UTC, UTC+0` rather than their zone.
- **Downgrade loses the timezone.** Decision 6. Acceptable and one re-save to fix, but it belongs in the release notes.
- **`JSON_DOC_TINY` is 128 bytes** (`Config.h:19`) and is the buffer for the timezone POST. ArduinoJson v6 deserialises zero-copy from the modifiable request buffer, so a 44-char string costs object overhead rather than 44 bytes — but the margin is thin enough that it should move to `JSON_DOC_SMALL`. The `/api/timers` GET response uses `JSON_DOC_LARGE` (1024 B) and gains roughly 80 bytes of new fields on top of four serialised timers; the overflow check is a task, not an assumption.

## Migration Plan

Single-step, automatic, on first boot of the new firmware. No user action required and no data loss except the DST-awareness the old format could not express anyway.

```
  old firmware          first boot, new firmware              steady state
  ────────────          ────────────────────────              ────────────
  tz_offset = 1    ──▶  isKey("tz") == false                  tz = "UTC-1"
                        legacyOffsetToPosix(1) -> "UTC-1"     (banner shown until
                        putString("tz", "UTC-1")               user picks a region)
                        remove("tz_offset")

  timer_0_dur =    ──▶  type == ALARM_DAILY                   timer_0_dur = 0
    29383470            -> duration_seconds = 0               timer_0_lfd = 0xFFFF
                        putUShort("timer_0_lfd", 0xFFFF)
```

The first alarm evaluation after migration sees `last_fired_yday == 0xFFFF`, which never equals a real `tm_yday`, so a migrated alarm is eligible to fire on its next occurrence — correct.

## Open Questions

- Should the advanced raw TZ field be visible by default, or behind a disclosure? Leaning disclosure — it is a footgun for the 99% and a necessity for the 1%.
- Is ~40 dropdown rows the right size? Fewer means more users hit the advanced field; more means a longer list to scroll on a phone. 40 covers every zone with a distinct rule that has meaningful population.
