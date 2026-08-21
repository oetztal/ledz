## 1. Generate the timezone table

- [x] 1.1 Write `scripts/gen_timezones.py`: for each chosen IANA zone, read the POSIX footer with `awk 'END{print}' /usr/share/zoneinfo/<zone>` (the last line of a TZif v2+ file) and emit the `<option value="…" data-iana="…">Label</option>` rows (design decision 2). Do **not** hand-type the strings
- [x] 1.2 Choose ~40 zones covering every distinct rule with meaningful population. Include the counterintuitive ones explicitly and verify each: Brazil `America/Sao_Paulo` (no DST since 2019), Mexico `America/Mexico_City` (none since 2022), Iran `Asia/Tehran` (none since 2022), Egypt `Africa/Cairo` (reinstated 2023), plus `Asia/Kolkata` (+5:30), `Asia/Kathmandu` (+5:45), `America/St_Johns` (−3:30), `Australia/Adelaide` (+9:30), `Australia/Sydney` and `Pacific/Auckland` (southern-hemisphere rules), `America/Phoenix` and `Australia/Brisbane` (no DST in a DST country)
- [x] 1.3 Sort rows by current standard offset so the list scans like the existing one, and label each by cities rather than by offset
- [x] 1.4 Record the generated table's length; confirm it is within a few KB of the 25 rows it replaces
  - 49 rows, 6,579 bytes raw, replacing 25 rows at 1,772 bytes — +4.8 KB raw. About 2.2 KB of that is the column alignment padding, which `minify_html`'s `\s+` → single-space pass removes before gzip, so the shipped delta is far smaller (measured in task 9.3)

## 2. `src/support/LocalTime` — the pure layer

- [x] 2.1 Create `src/support/LocalTime.h` with the interface from design decision 9: `secondsSinceMidnight`, `localDayOfYear`, `describe`, `legacyOffsetToPosix`, `isSyntacticallyValidTz`, and the `Info { int16_t offset_minutes; bool is_dst; char abbrev[8]; }` struct
- [x] 2.2 Implement `applyTz(const char*)` internally: compare against the last-applied string, and only on a difference call `setenv("TZ", tz, 1)` then `tzset()`. Always call `tzset()` after `setenv` — POSIX does not require `localtime_r` to do it (design decision 9)
- [x] 2.3 `describe()` fills `offset_minutes` from `tm_gmtoff / 60`, `is_dst` from `tm_isdst > 0`, `abbrev` from `tm_zone` (bounded copy into the 8-byte buffer)
- [x] 2.4 `legacyOffsetToPosix(hours, out, len)` → `snprintf(out, len, "UTC%+d", -hours)`. **The negation is the point** — old `+1` becomes `UTC-1`, which is UTC+1 (design decisions 1 and 5)
- [x] 2.5 `isSyntacticallyValidTz`: non-empty, ≤63 chars, printable ASCII, no `=`, and either three leading alphabetic characters or a leading `<…>` designator (design decision 4)
- [x] 2.6 Confirm `src/support/` needs no `platformio.ini` change — `+<support/>` is already in the native filter at line 14
- [x] 2.7 Header comment: state that these functions mutate process-global `TZ`, and that callers must observe design decision 4

## 3. Native tests

- [x] 3.1 Create `test/test_localtime/test_localtime.cpp`
- [x] 3.2 Assert the four `secondsSinceMidnight` scenarios from the spec: Berlin winter/summer, the 2026-03-29T00:59:59Z → 01:59:59 and +1s → 03:00:00 transition pair, `IST-5:30`, and `<-03>3` across the year
- [x] 3.3 Assert `legacyOffsetToPosix` for **every** value −12…+14 and check the resulting effective offset equals the original — this is the guard against the POSIX sign inversion
- [x] 3.4 Table test: for every row emitted by `scripts/gen_timezones.py`, assert the effective offset at 2026-01-15 and 2026-07-15 matches the offset the host's own tzdata reports for the corresponding IANA zone. Catches a mistyped or stale POSIX string
- [x] 3.5 Assert `localDayOfYear` returns the same value at both 02:30 instants of a Berlin fall-back day, and different values across an ordinary local midnight
- [x] 3.6 Assert `isSyntacticallyValidTz` rejects empty, 64+ chars, embedded `=`, non-printable bytes; accepts `UTC0`, `IST-5:30`, `<+0545>-5:45`, `CET-1CEST,M3.5.0,M10.5.0/3`
- [x] 3.7 Tests must not assume `TZ` is unchanged across a call (design decision 9); set it explicitly in each case rather than relying on order
- [x] 3.8 `pio test -e native` passes
  - 14/14 in `test_localtime`, 109/109 across the whole native suite

## 4. Config: struct and NVS

- [x] 4.1 `src/Config.h:163` — replace `int8_t timezone_offset_hours` with `char timezone[64]`, defaulted to `"UTC0"`. 64 is sized off Chatham's 44-character string (design decision 1)
- [x] 4.2 `src/Config.h:148-155` — add `uint16_t last_fired_yday = 0xFFFF` to `TimerEntry`; add a named constant for the 0xFFFF "never" sentinel. Restore `duration_seconds`' comment to mean only the countdown duration (design decision 7)
- [x] 4.3 `src/Config.cpp:400` — replace `getChar("tz_offset")` with the migration branch from design decision 6: `isKey("tz")` → `getString`, else convert the legacy offset and set a `migrated` flag
- [x] 4.4 In the per-timer loop, read `timer_%u_lfd` via `getUShort`, defaulting to the never-sentinel. Key length is 11, within the 15-character NVS limit
- [x] 4.5 When `migrated`, zero `duration_seconds` for every `ALARM_DAILY` entry — existing devices have a trigger-minute stored there (design decision 6)
- [x] 4.6 When `migrated`, call `saveTimersConfig` then `prefs.remove("tz_offset")`. Note `loadTimersConfig` opens NVS read-only, so the write-back needs its own read-write session after `prefs.end()`
- [x] 4.7 `src/Config.cpp:435` — `saveTimersConfig` writes `putString("tz", …)` and `putUShort("timer_%u_lfd", …)`; the `tz_offset` write is removed
- [x] 4.8 Verify migration idempotency and crash-safety by hand against the three interruption points in design decision 6
  - Interrupted before write-back: `tz` absent, `tz_offset` present -> next boot re-runs the same pure conversion, same result.
  - Interrupted between `saveTimersConfig` and `remove("tz_offset")`: next boot takes the `isKey("tz")` read path. The removal is therefore driven by `legacyKeyPresent` rather than by `migrated`, so the orphan is still cleared on that boot.
  - Second boot after success: `tz` present, `tz_offset` absent -> read path, no write.

## 5. TimerScheduler

- [x] 5.1 `TimerScheduler.cpp:32-45` — `getSecondsSinceMidnight` delegates to `LocalTime::secondsSinceMidnight(epoch, timersConfig.timezone)`; delete the hand-rolled offset arithmetic and the negative-epoch branch
- [x] 5.2 `TimerScheduler.h:105,111` — `setTimezoneOffset(int8_t)` / `getTimezoneOffset()` become `setTimezone(const char*)` / `getTimezone()`
- [x] 5.3 `setTimezone` validates via `LocalTime::isSyntacticallyValidTz`, copies into `timersConfig.timezone`, saves, and sets a `tzDirty` flag — it does **not** call `setenv`/`tzset` itself (design decision 4)
- [x] 5.4 At the top of `checkTimers`, if `tzDirty`, apply the TZ and log the resulting abbreviation and offset from `LocalTime::describe` — this is the only diagnostic for a syntactically-valid-but-wrong string (design decision 4)
- [x] 5.5 `TimerScheduler.cpp:77-91` — replace the `currentEpoch / 60` dedup marker with `last_fired_yday`: compute the local day once per `checkTimers` call, and inside the alarm window trigger only when `timer.last_fired_yday != today`, then store `today` (design decision 7)
- [x] 5.6 Confirm the `>= target && < target + 5` window is left as-is — it is what makes the spring-forward gap skip the alarm, which is the specified behaviour (design decision 8)
  - Left as-is at `TimerScheduler.cpp:88-89`; comment added recording that the skipped hour is the specified behaviour
- [x] 5.7 `TimerScheduler.cpp:227` — `getRemainingSeconds` for `ALARM_DAILY` still uses `getSecondsSinceMidnight`; verify the next-occurrence arithmetic is unchanged and note that it is a wall-clock estimate that can be off by an hour on a transition day
  - Arithmetic unchanged; comment added noting it counts wall-clock rather than real seconds, so it is an hour out on a transition day. UI-only — the trigger itself compares wall-clock time directly
- [x] 5.8 `TimerScheduler.cpp:18-19` — update the boot log to print the TZ string
- [x] 5.9 `setDailyAlarm` initialises `last_fired_yday` to the never-sentinel so a freshly set alarm can fire today

## 6. Network ordering

- [x] 6.1 Confirm `configTime(0, 0, …)` at `Network.cpp:208` still runs before any TZ apply. Keeping the apply inside `checkTimers` (task 5.4) gets this for free, since `checkTimers` is only reachable after `configTime` — verify rather than assume
  - Verified: `configTime` at `Network.cpp:208`, `timerScheduler->begin()` at `:222` only sets `tzDirty`, and the first `checkTimers` (the only place that applies TZ) runs later from the task loop at `:368`
- [x] 6.2 Set `tzDirty` at scheduler `begin()` so the stored TZ is applied on the first `checkTimers` iteration after boot
- [x] 6.3 Confirm the comment at `Network.cpp:200-207` still holds: `time()`/`gettimeofday()` stay UTC regardless of `TZ`, so mbedtls certificate validity is unaffected. Add a sentence recording that `TZ` is now non-UTC and why that is safe

## 7. HTTP API

- [x] 7.1 `WebServerManager.cpp:860` — replace `timezone_offset_hours` with `timezone`, and add `tz_abbrev`, `tz_offset_minutes` and `is_dst` from `LocalTime::describe(currentEpoch, tz)`
- [x] 7.2 Guard the new fields when `currentEpoch == 0` (no NTP): report the stored `timezone` but omit or zero the derived fields rather than describing epoch 0
- [x] 7.3 Check the `JSON_DOC_LARGE` (1024 B) budget with four timers plus ~80 bytes of new fields; bump if `doc.overflowed()` (design: Risks)
  - Measured with the real ArduinoJson: the worst case (4 enabled timers, Chatham's 44-char string) is 47 slots. `sizeof(VariantSlot)` is 16 on the 32-bit ESP32, so 752 of 1024 bytes — 27% headroom, no bump needed.
  - Fixed while checking: `tz_abbrev` was assigned from a local that died before `serializeJson`. ArduinoJson stores a `const char*` by reference, so the `Info` is now hoisted out of the `if`.
- [x] 7.4 `WebServerManager.cpp:1139-1152` — the POST body key becomes `tz`; validate with `LocalTime::isSyntacticallyValidTz` and return `400` with a specific error for missing versus invalid
- [x] 7.5 Raise the POST buffer from `JSON_DOC_TINY` (128 B) to `JSON_DOC_SMALL` — the margin for a 44-character string is too thin (design: Risks)
- [x] 7.6 Confirm no other caller of the removed `getTimezoneOffset()` remains (`grep -rn timezone_offset_hours src/` should come back empty)
  - `grep -rn 'timezone_offset_hours\|getTimezoneOffset\|setTimezoneOffset' src/` is empty

## 8. Timers page

- [x] 8.1 `data/timers.html:27-53` — replace the 25 hour options with the generated table from task 1.1; the `<select>` keeps `id="timezoneOffset"`, or is renamed to `id="timezone"` with all three JS references updated (`:199`, `:435`, and the new load path)
- [x] 8.2 `:198-200` — read `data.timezone` and set `select.value` directly. Detect the no-match case: it means either a legacy fixed offset or an advanced string
- [x] 8.3 `:23` — the heading becomes `Current Time (${tz_abbrev}, UTC${sign}${hh}:${mm})` from `tz_offset_minutes`, replacing the static `UTC` + `tzDisplay` span
- [x] 8.4 Add the Detect button and its three-step matching from design decision 3: exact `data-iana`, then the two-probe offset fallback (now and +6 months), then the "couldn't match" message. It selects only — it must not POST
- [x] 8.5 Add the advanced raw TZ string field behind a disclosure (design: Open Questions). When the stored value matches no option and is not a legacy fixed offset, show it here and open the disclosure
- [x] 8.6 Add the fixed-offset banner: shown when the stored value matches `^UTC[+-]?\d+$` and no option, with the wording from design decision 5
- [x] 8.7 `:434-…` — `setTimezone()` POSTs `{"tz": …}`, taking the advanced field's value when the disclosure is open and non-empty, otherwise the select's. On success, re-fetch `/api/timers` after a short delay so the header picks up the applied abbreviation (design decision 4)
- [x] 8.8 Surface a POST failure to the user rather than only `console.error`
- [x] 8.9 Confirm the client-side clock extrapolation at `:225` (`serverTimeAtFetch + elapsed`) is left alone — it is wrong across a transition, but only for a page left open through 2 a.m., and the periodic re-fetch corrects it
  - Left alone

## 9. Build

- [x] 9.1 Record the current size of `src/generated/timers_gz.h`
  - 3,634 bytes compressed (21,360 raw / 13,731 minified), file 23,041 bytes
- [x] 9.2 `pio run -e adafruit_qtpy_esp32s3_nopsram` — the pre-build step regenerates it via `scripts/compress_web.py`
- [x] 9.3 Compare sizes and confirm the growth is roughly the table delta plus the new JS
  - 6,214 bytes compressed (34,223 raw / 21,735 minified): +2,580 compressed. Consistent with ~2.3 KB of extra table rows plus ~4 KB of new JS and markup after minification
- [x] 9.4 Confirm no `calc()` with `+`/`-` in any CSS added by this change — `scripts/compress_web.py:38` strips the required whitespace and silently produces invalid CSS
  - No `calc()` in any CSS added by this change
- [x] 9.5 Check the flash-usage delta against the partition budget in `partitions.csv`
  - Flash 1,241,029 -> 1,245,713 bytes, +4,684. app0 is 1856K, leaving 654,831 bytes free (65.5% used)

## 10. Manual verification

There is no browser or hardware test harness; everything below is manual.

- [ ] 10.1 **Migration.** Flash the *old* firmware, set the timezone to UTC+1, set a daily alarm, then OTA to the new firmware. Confirm the page reports `UTC-1`, the clock is unchanged, the fixed-offset banner appears, and the alarm survives
- [ ] 10.2 Reboot and confirm the migration does not run again (log shows the read path, not the convert path)
- [ ] 10.3 Confirm `tz_offset` is gone from NVS and `tz` is present
- [ ] 10.4 Pick "Berlin" from the list, save, and confirm the header switches to `CET`/`CEST` with the right offset within a second or two
- [ ] 10.5 Press Detect on a machine in a listed zone and confirm it preselects the right row and sends nothing until Save
- [ ] 10.6 Press Detect with the browser forced to an unlisted zone (`TZ=Pacific/Chatham` for the dev server, or an OS change) and confirm the fallback or the "couldn't match" message
- [ ] 10.7 Enter a nonsense advanced string, save, and confirm the header shows `UTC, UTC+0` and the log records the applied abbreviation — the documented failure mode (design: Risks)
- [ ] 10.8 Enter `IST-5:30` and confirm the clock shows a half-hour offset
- [ ] 10.9 **Fall-back double-fire.** Set the device clock so a daily alarm sits inside a fall-back repeated hour and confirm exactly one trigger. If the clock cannot be forced, exercise `LocalTime::localDayOfYear` in the native test (task 3.5) and inspect the scheduler log across a real transition
- [ ] 10.10 **Spring-forward gap.** Set an alarm inside the skipped hour and confirm it does not fire that day and does fire the next
- [ ] 10.11 Confirm a running countdown timer is unaffected by a timezone change mid-countdown
- [ ] 10.12 Disconnect NTP and confirm the page still shows "NTP not available" and the timezone controls still work
- [ ] 10.13 Run an OTA update after setting a non-UTC timezone and confirm the TLS handshake still succeeds (guards task 6.3)
- [ ] 10.14 Downgrade to the previous firmware and confirm it boots with the timezone reset to UTC rather than misbehaving (design decision 6) — then note this in the release notes
