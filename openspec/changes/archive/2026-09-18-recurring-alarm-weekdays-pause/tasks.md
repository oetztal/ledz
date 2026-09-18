## 1. `LocalTime` — weekday helper (host-tested)

- [x] 1.1 Add `uint8_t localWeekday(uint32_t epoch, const char *tz)` to `src/support/LocalTime.h`, documented as returning `tm_wday` (Sunday = 0) from the same conversion as `localDayOfYear`
- [x] 1.2 Implement it in `src/support/LocalTime.cpp` via the existing `localParts(epoch, tz)`
- [x] 1.3 `test/test_localtime`: assert weekday for a known Monday and Sunday in `CET-1CEST,M3.5.0,M10.5.0/3`; assert both 02:30 instants of a Berlin fall-back night (last Sunday of October) return Sunday and the same `localDayOfYear`; assert a UTC instant late on Saturday that is Sunday in `<+13>-13` returns Sunday
- [x] 1.4 `pio test -e native` passes

## 2. Config: struct and NVS

- [x] 2.1 `src/Config.h`: add `constexpr uint8_t SCHEDULE_EVERY_DAY = 0x7F`; add `bool paused = false` and `uint8_t days_mask = SCHEDULE_EVERY_DAY` to `TimerEntry` with comments (bit *n* = `tm_wday` *n*; `paused` meaningful for `SCHEDULE` only); reword the `enabled` comment to "slot occupied"
- [x] 2.2 `src/Config.cpp` `loadTimersConfig`: inside the `if (enabled)` block read `timer_%u_paused` with `getBool(key, false)` and `timer_%u_days` with `getUChar(key, SCHEDULE_EVERY_DAY)` (design decision 6)
- [x] 2.3 `src/Config.cpp` `saveTimersConfig`: write both keys for every occupied slot; confirm `key[20]` still fits `timer_%u_paused` (14 chars)
- [x] 2.5 `MAX_TIMERS` 4 → 12; `enabled` NVS key shortened to `timer_%u_en` with fallback read of `timer_%u_enabled` and removal on save (15-character NVS key limit)
- [x] 2.4 Verify by hand: an existing schedule on a device flashed from `main` loads as armed, every day, after the update

## 3. TimerScheduler

- [x] 3.1 `setSchedule(index, secondsSinceMidnight, action, presetIndex, daysMask)`: reject `daysMask == 0` or `> 0x7F`; if the slot already holds an `SCHEDULE`, keep its `paused` value, otherwise set `paused = false`; store `days_mask`; keep resetting `last_fired_yday` (design decision 5)
- [x] 3.2 Add `bool setPaused(uint8_t index, bool paused)`: return false for out-of-range, empty, or `COUNTDOWN` slots; otherwise set the flag, persist via `saveTimersConfig`, log, return true. Must not touch `last_fired_yday` (design decision 4)
- [x] 3.3 `checkTimers`: compute `const uint8_t wday = LocalTime::localWeekday(currentEpoch, tz)` once per tick next to `today`; in the `SCHEDULE` case require `!timer.paused` and `(timer.days_mask >> wday) & 1` before the existing window and `last_fired_yday` checks
- [x] 3.4 `getRemainingSeconds` for `SCHEDULE`: return 0 when paused; otherwise compute seconds to today's or tomorrow's `target_time`, then advance whole days (and `wday`) until the mask bit is set, at most seven steps (design decision 8)
- [x] 3.5 Header comments updated for the new parameter and method; `cancelTimer` leaves `paused`/`days_mask` at defaults when clearing a slot

## 4. HTTP API

- [x] 4.1 `POST /api/timers/schedule` (alias `/api/timers/alarm`): read optional `days` (default 127); 400 `{"success":false,"error":"Invalid days"}` for 0 or >127; pass to `setSchedule`
- [x] 4.2 New `POST /api/timers/pause` handler (`AsyncCallbackJsonWebHandler`, `HTTP_POST`): require `index` and `paused`; 400 for missing fields, out-of-range index, empty slot or countdown (surface `setPaused`'s false as 400 with a descriptive error); 200 `JSON_RESPONSE_SUCCESS` otherwise
- [x] 4.3 `GET /api/timers`: add `paused` and `days_mask` to each occupied slot's object
- [x] 4.4 Manual `curl` pass against a device covering every scenario in the "exposed over HTTP" requirement: create with `days:62`, omit `days`, `days:0`, pause schedule, pause countdown, pause empty slot, update by index keeps `paused`

## 5. Timers page — form and day picker

- [x] 5.1 `data/common.css`: add `.day-picker` (flex row) and `.day-picker button` styles: ≥44 px touch target, pressed state via `[aria-pressed="true"]`, `:focus-visible` ring, no inline layout styles; add `.day-dots` and `.day-dots span[data-on]` filled/hollow styles plus the small letter row
- [x] 5.2 `data/timers.html`: add a "Repeat" `.form-group` with seven `<button type="button" class="day-btn" data-wday="1..6,0" aria-pressed="true" aria-label="Monday"…>` in Monday-first order, initials as text
- [x] 5.3 JS: one helper pair `maskToDays(mask)` / `daysToMask(buttons)` using `data-wday` so the Sunday-first mask and Monday-first DOM never drift; click handler flips `aria-pressed`
- [x] 5.4 `setSchedule()`: include `days` from the picker; alert on an empty selection before sending; send `index` when `editingIndex !== null`
- [x] 5.5 Edit mode: `let editingIndex = null`; `startEdit(index)` fills time, action, preset, picker from `timersData`, sets the submit label to "Update Schedule" and shows a "cancel edit" link; `cancelEdit()` resets to defaults; a successful submit calls `cancelEdit()` then `fetchTimers()`

## 6. Timers page — schedule cards

- [x] 6.1 Card label `SCHEDULE` (green) when armed, `PAUSED` (grey) when paused; paused card gets a `.paused` class that dims it
- [x] 6.2 Render the `.day-dots` row from `timer.days_mask` beneath the time, with the M T W T F S S letter row
- [x] 6.3 Add an armed toggle switch (reuse the existing switch component and its `id`/`checked` conventions) wired to `POST /api/timers/pause`; on failure restore `checked` and re-fetch
- [x] 6.4 Add an Edit button calling `startEdit(timer.index)`; keep Cancel as is
- [x] 6.5 Only re-render `timerList.innerHTML` on the one-second tick when a countdown is present, so the toggle is not rebuilt mid-interaction (design risk)
- [x] 6.6 Sort the card list: countdowns first by remaining time, then schedules by `target_time`, slot index as tie-break
- [x] 6.7 Regenerate `src/generated/timers_gz.h` via the normal build (`scripts/compress_web.py` pre-step) and note the size delta

## 7. Verification

- [x] 7.1 On device: create a Mon–Fri schedule, confirm dots and label; set the device clock scenario (or set a schedule one minute ahead on the current weekday and again with today's bit cleared) and confirm it fires only when today is selected
- [x] 7.2 On device: pause a schedule one minute ahead, confirm no firing; resume before the time, confirm firing; pause after firing then resume the same day, confirm no second firing
- [x] 7.3 On device: edit a paused schedule, confirm it stays paused and the slot index is unchanged; edit an armed schedule to a time later today, confirm it fires today
- [x] 7.4 Reboot with a paused schedule and a Mon–Fri schedule present; confirm both states survive
- [x] 7.5 Keyboard pass over the day picker and the card toggle; check focus rings and Space/Enter behaviour
- [x] 7.6 `pio test -e native` and the embedded build both green

## 8. Docs and specs

- [x] 8.0 Rename user-facing and internal "alarm" wording to "schedule"; keep `/api/timers/alarm` and `type_name: "alarm_daily"` as stable wire aliases

- [x] 8.1 README "Timers & Schedules" feature line: mention weekday selection and pause
- [x] 8.2 Run `openspec validate` on this change; fix any spec-format findings before archive
