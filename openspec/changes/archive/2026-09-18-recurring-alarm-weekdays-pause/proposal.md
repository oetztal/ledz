## Why

A schedule on the timers page is fire-and-forget: once set, the only thing the user can do with it is cancel it. Changing the time, the action or the preset means cancelling and re-creating it, and there is no way to keep a schedule but silence it for a while (holidays, a weekend lie-in). Every schedule also fires seven days a week; "weekday mornings only" is not expressible, which is the most common schedule shape there is.

The backend is closer than the UI suggests. `POST /api/timers/schedule` already accepts an optional `index` and overwrites that slot in place (`WebServerManager.cpp:906-980`), so "update" is mostly a missing button. Weekday selection and pause, however, need one new field each on `Config::TimerEntry` and a change to the firing rule in `TimerScheduler::checkTimers()`.

## What Changes

- **Schedules can be edited in place.** Each schedule card on the timers page gets an **Edit** button that loads the schedule into the form; the submit button becomes "Update Schedule" and the request reuses the existing `index` path of `POST /api/timers/schedule`. Updating keeps the slot and keeps the schedule's paused state.
- **Schedules can be restricted to weekdays.** `TimerEntry` gains a 7-bit `days_mask` (bit *n* = `tm_wday` *n*, Sunday = 0; `0x7F` = every day). The firing rule additionally requires today's local weekday to be selected. `POST /api/timers/schedule` gains an optional `days` field (default every day; an empty mask is rejected). `GET /api/timers` reports `days_mask`.
- **Schedules can be paused without being deleted.** `TimerEntry` gains a `paused` flag. A paused schedule keeps all its settings and stays listed, but never fires. New endpoint `POST /api/timers/pause {index, paused}`; `GET /api/timers` reports `paused`. Pause and resume never touch the last-fired record, so resuming after today's firing does not fire again today. Pause applies to schedules only; countdown timers reject it.
- **The remaining-time estimate honours the mask.** `getRemainingSeconds()` for a schedule scans up to seven days ahead for the next selected weekday instead of assuming tomorrow.
- **Timers page.** The schedule form gets a Monday-first row of seven day-letter toggle buttons. Schedule cards show a Monday-first row of seven dots (filled = selected) under the time, an armed/paused toggle switch, Edit and Cancel. Paused cards are dimmed and labelled `PAUSED`; the `DAILY` label becomes `SCHEDULE` since schedules are no longer necessarily daily.
- **Persistence is additive.** Two new NVS keys per slot, `timer_%u_paused` and `timer_%u_days`, with load defaults of `false` and `0x7F`. Existing schedules therefore behave exactly as before after the update, with no explicit migration step.
- **Twelve slots instead of four.** `TimersConfig::MAX_TIMERS` becomes 12. Two-digit indices push `timer_%u_enabled` past the 15-character NVS key limit, so the key becomes `timer_%u_en`; the old key is read as a fallback and removed on the next save, so existing slots survive the update.
- **`LocalTime` gains `localWeekday(epoch, tz)`**, reading `tm_wday` from the same `localtime_r` call as `localDayOfYear`, so the weekday and the day-of-year used for de-duplication always agree, including on a fall-back night.

Nothing here is **BREAKING**: every new request field is optional, every new response field is additive, and the existing `enabled` flag keeps its meaning of "slot occupied".

## Capabilities

### New Capabilities
- *(none)*

### Modified Capabilities
- `timer-scheduling`: The requirement "A daily alarm fires exactly once per local day" is renamed to "A schedule fires exactly once per local day" (the feature is called a *schedule* throughout the UI, docs and code from this change on; the HTTP path `/api/timers/alarm` and `type_name: "alarm_daily"` stay as aliases) and generalised to a schedule that fires at most once per local day, only on selected weekdays, and never while paused. New requirements cover the weekday mask, the paused state and its interaction with the last-fired record, editing a schedule in place, the `POST /api/timers/pause` endpoint, the `days` request field, the new `GET /api/timers` fields, and the mask-aware remaining-time estimate.
- `web-ui-controls`: New requirements for the weekday picker (a group of seven toggle buttons rather than seven checkboxes) and the weekday dots indicator, and the schedule card's armed/paused toggle switch as an instance of the existing "immediate effect → toggle switch" rule.

## Impact

- `src/Config.h` — `TimerEntry` gains `bool paused` and `uint8_t days_mask`; constant `SCHEDULE_EVERY_DAY = 0x7F`; the `enabled` comment states "slot occupied"
- `src/Config.cpp` — `loadTimersConfig`/`saveTimersConfig` read and write `timer_%u_paused` and `timer_%u_days` with the defaults above
- `src/support/LocalTime.{h,cpp}` — new `localWeekday(epoch, tz)`
- `src/TimerScheduler.{h,cpp}` — `setSchedule` gains a `daysMask` parameter and preserves `paused` on an occupied schedule slot; new `setPaused(index, paused)`; `checkTimers` checks `paused` and the mask; `getRemainingSeconds` scans ahead
- `src/WebServerManager.cpp` — `days` on `POST /api/timers/schedule`; new `POST /api/timers/pause`; `paused` and `days_mask` on `GET /api/timers`
- `data/timers.html` — day picker in the form, edit mode, card redesign (dots, toggle, Edit); `data/common.css` for the shared day-button and dot styles
- `src/generated/timers_gz.h` — regenerated by `scripts/compress_web.py` in the pre-build step
- `test/test_localtime/` — tests for `localWeekday`
- `openspec/specs/timer-scheduling/spec.md`, `openspec/specs/web-ui-controls/spec.md` — delta specs

Low risk. The firing rule gains two `&&` terms; the NVS change is additive with behaviour-preserving defaults; and `TimerScheduler.cpp` is not in the native build filter, so its new branches are verified on device rather than by host tests (see design).

## Non-goals

- **Pausing countdown timers.** That would mean freezing remaining time, a different feature; the endpoint rejects countdown indices.
- **Multiple times per schedule or per-day times.** One time per slot; use another of the four slots.
- **Catch-up firing** after resume for an occurrence missed while paused.
- **Renaming `enabled`.** It keeps meaning "slot occupied"; `paused` is a separate flag (design decision 1).
