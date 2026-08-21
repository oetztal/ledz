## Why

All local-time reasoning on the device is one line — `TimerScheduler.cpp:36`:

```cpp
int32_t localEpoch = static_cast<int32_t>(epochTime) + (timersConfig.timezone_offset_hours * 3600);
```

`timezone_offset_hours` is an `int8_t` (`Config.h:163`), persisted as NVS `tz_offset` (`Config.cpp:400`), set by `POST /api/timers/timezone {"offset": n}` (`WebServerManager.cpp:1116`) from a 25-entry whole-hour dropdown (`timers.html:27-53`). Three defects follow:

- **No daylight saving.** A daily alarm set for 07:00 in Berlin fires at 08:00 all summer unless the user remembers to re-save their timezone twice a year.
- **Whole hours only.** India (+5:30), Nepal (+5:45), Newfoundland (−3:30), Adelaide and Chatham cannot be expressed at all.
- **The user is asked for an offset, not a place.** They have to know their *current* UTC offset rather than where they live.

The blast radius is narrower than it looks. Countdown timers store an absolute epoch in `target_time` and are DST-immune; only `ALARM_DAILY` (wall-clock seconds since midnight) and the clock display on the timers page are affected.

But DST does not merely shift the clock — it breaks the alarm de-duplication at `TimerScheduler.cpp:84`, which keys on `currentEpoch / 60`:

```
Spring forward   01:59 ──▶ 03:00      a 02:30 alarm has no instant → never fires
Fall back        02:59 ──▶ 02:00      02:30 occurs at two distinct epochs
                                      → two distinct dedup keys → FIRES TWICE
```

So the lights would come on twice on one October night. That is a visible misbehaviour introduced the moment DST support lands, and it has to be fixed in the same change.

Meanwhile `Config::TimerEntry::duration_seconds` is documented as "original duration for countdown display" but is overwritten with a trigger-minute marker for daily alarms (`TimerScheduler.cpp:86`) — one field doing two jobs, and the reason the dedup key is epoch-based in the first place.

## What Changes

- **A POSIX TZ string replaces the hour offset as the stored representation.** newlib on ESP-IDF already implements the full POSIX `TZ` grammar including transition rules, so `CET-1CEST,M3.5.0,M10.5.0/3` buys correct DST, sub-hour offsets and the zone abbreviation from code already linked into the binary. `getSecondsSinceMidnight()` becomes `localtime_r()`.
- **The zone table lives in `data/timers.html`, not in firmware.** The `<option>` **value** *is* the POSIX string, so the device stores and returns exactly what the dropdown offers and `select.value = data.timezone` re-selects the right row with no lookup. No `/api/timezones` endpoint, no C++ table.
- **A "Detect" button** reads `Intl.DateTimeFormat().resolvedOptions().timeZone` and matches it against a `data-iana` attribute on each option. It only *selects*; Save still commits.
- **An "Advanced" raw TZ string field** is the escape hatch for zones not in the table and for rule changes that would otherwise need an OTA.
- **`GET /api/timers` gains `timezone`, `tz_abbrev`, `tz_offset_minutes` and `is_dst`**, so the header can read `Current Time (CEST, UTC+2)` instead of the static `(UTC+1)`. `POST /api/timers/timezone` takes `{"tz": "<posix>"}` instead of `{"offset": n}`.
- **NVS migration.** A new `tz` string key replaces `tz_offset`; a new `timer_%u_lfd` key stores the last-fired local day for each alarm. Migration runs on read, writes back, and removes the legacy key. Legacy offsets migrate to the **behaviour-preserving fixed-offset** form (`UTC-1` for the old `+1`), not to a guessed region — see design decision 5. The UI recognises that shape and prompts the user to pick their region.
- **Fall-back double-fire is fixed** by replacing the epoch-minute dedup marker with `last_fired_yday`, which also frees `duration_seconds` to mean only what its comment says.
- **The spring-forward gap is documented, not papered over.** An alarm whose wall-clock second is skipped by the transition does not fire that day.
- **The conversion logic moves to `src/support/`**, which is already in the native build filter (`platformio.ini:14`), so the transitions and the migration table become host-testable without flashing.

## Capabilities

### New Capabilities
- `timer-scheduling`: Describes how the device derives local time from UTC, how the timezone is configured, persisted and migrated, and when a daily alarm fires — including its behaviour across both daylight-saving transitions.

### Modified Capabilities
- *(none)* — no existing spec covers timers. `settings-page` and `web-ui-controls` are untouched; the timers page's timezone controls are new markup on a page neither spec describes.

## Impact

- `src/support/LocalTime.{h,cpp}` — **new.** `secondsSinceMidnight(epoch, tz)`, `localDayOfYear(epoch, tz)`, `describe(epoch, tz) -> {abbrev, offset_minutes, is_dst}`, `legacyOffsetToPosix(int8_t)`, `isSyntacticallyValidTz(const char*)`
- `src/Config.h` — `TimersConfig::timezone_offset_hours` → `char timezone[64]`; `TimerEntry` gains `uint16_t last_fired_yday`
- `src/Config.cpp` — `loadTimersConfig`/`saveTimersConfig` read/write `tz` and `timer_%u_lfd`; one-shot migration from `tz_offset` and from the clobbered `timer_%u_dur`
- `src/TimerScheduler.{h,cpp}` — `getSecondsSinceMidnight` delegates to `LocalTime`; `setTimezoneOffset` → `setTimezone(const char*)`; alarm dedup keyed on local day; TZ applied to the process from `checkTimers`
- `src/Network.cpp` — `configTime(0, 0, …)` (`:208`) must not clobber the configured TZ; ordering fixed
- `src/WebServerManager.cpp` — `/api/timers` response fields; `/api/timers/timezone` body contract; `JSON_DOC_TINY` (128 B) is too tight for the new body
- `data/timers.html` — timezone `<select>` rebuilt with POSIX values and `data-iana`; Detect button; advanced raw field; legacy-offset nudge; header shows abbreviation and effective offset
- `src/generated/timers_gz.h` — regenerated by `scripts/compress_web.py` in the pre-build step
- `test/test_localtime/` — **new** native tests

Moderate risk, concentrated in two places: the POSIX sign inversion (`CET-1` means UTC**+**1) and the NVS migration. Both are covered by native tests. `time()`/`gettimeofday()` remain UTC regardless of `TZ`, so the mbedtls certificate-validity behaviour documented at `Network.cpp:200-207` is unaffected.

## Non-goals

- **On-device tzdata.** The IANA database is ~500 KB; a POSIX string is 64 bytes and expresses every rule these zones currently use.
- **Automatic rule updates.** If a jurisdiction changes its DST rule, the fix is a firmware update or the advanced raw field — not a network lookup.
- **Any online timezone/geolocation service.** It would add a network dependency and a privacy surface to something that is a pure function of a 30-byte string.
- **Catch-up firing for alarms skipped by the spring-forward gap.** One alarm, one day a year, at 2 a.m.; see design decision 8.
- **A general NVS schema-version mechanism.** This change migrates its own keys; versioning the whole config is a separate concern (design decision 6).
- **Moving the timezone control to the settings page.** It stays on the timers page where it is today.
