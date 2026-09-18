## Context

Schedule state lives in `Config::TimerEntry` (`Config.h:145-156`), four slots in `TimersConfig` (twelve after this change), persisted as per-slot NVS keys by `ConfigManager::saveTimersConfig()` (`Config.cpp:476-509`). `TimerScheduler::checkTimers()` runs once per Network-task iteration and fires an `SCHEDULE` entry when:

```
enabled
&& secondsSinceMidnight(now) ∈ [target_time, target_time + 5)
&& last_fired_yday != localDayOfYear(now)
```

`enabled` currently carries two meanings at once: "this slot holds a timer" and "this timer is armed". The save routine leans on the first meaning and writes only the `enabled` key for a disabled slot, dropping every other field:

```
  slot state today          NVS after save
  ─────────────────────     ─────────────────────────────────
  enabled = true            timer_N_enabled, _type, _action,
                            _preset, _target, _dur, _lfd
  enabled = false           timer_N_enabled = false   (only)
```

So "paused" cannot be expressed as `enabled = false` without losing the schedule.

On the HTTP side, `POST /api/timers/schedule` already takes an optional `index` and calls `setSchedule(index, …)`, which overwrites the slot and resets `last_fired_yday`. `GET /api/timers` serialises each slot; the timers page (`data/timers.html`) renders one card per entry with a single Cancel button and re-fetches after any mutation.

`LocalTime` (`src/support/`) is in the native build filter and host-tested; `TimerScheduler.cpp` is not.

## Goals / Non-Goals

**Goals:**
- Edit an existing schedule without losing its slot or its paused state
- Restrict a schedule to a set of weekdays, evaluated in local time and consistent with the day-of-year de-duplication
- Pause and resume a schedule with no loss of settings and no spurious re-fire on resume
- Zero-step upgrade: existing schedules behave identically after flashing
- Weekday determination is host-tested

**Non-Goals:**
- Pausing countdown timers
- Catch-up firing for occurrences missed while paused or on unselected days
- Renaming or re-purposing `enabled`
- An NVS schema version

## Decisions

### 1. `paused` is a new flag; `enabled` keeps meaning "slot occupied"

Two options were weighed:

| | A. add `paused` | B. rename `enabled`→`occupied`, add `armed` |
|---|---|---|
| Struct change | +1 field | +1 field, 1 rename |
| Code touched | one `continue` in `checkTimers`, one key in load/save, one JSON field | every `timer.enabled` read in scheduler, config, web server, UI |
| NVS | one new key | one new key plus a key rename or alias |
| Reads as | "enabled but paused" | cleaner |

A wins on blast radius; the naming wrinkle is fixed with a struct comment (`enabled // slot occupied`). The persistence rule becomes: **all fields are written for an occupied slot, regardless of `paused`**, which is already what the code does since it keys on `enabled`.

`paused` is meaningful only for `SCHEDULE`. For countdown entries it is always `false`, and `setPaused()` refuses a countdown index rather than inventing a freeze semantic.

### 2. Weekdays are a 7-bit mask in `tm_wday` order

```
  bit:   6   5   4   3   2   1   0
  day:  Sat Fri Thu Wed Tue Mon Sun
  0x7F  every day    (default, legacy behaviour)
  0x3E  Mon–Fri
  0x41  Sat+Sun
```

Bit *n* = `tm_wday` *n* (Sunday = 0) so the firing check is `(days_mask >> wday) & 1` with no translation table. The Monday-first *display* order is a UI concern and is mapped in JavaScript only. `days_mask` is stored on every schedule; countdown entries carry `0x7F` and ignore it.

A mask of `0` is rejected at the API with 400 rather than treated as "paused": pause is an explicit, separate state (decision 1), and a silent never-fires schedule with all dots hollow would be indistinguishable from a UI bug.

### 3. The firing rule gains two terms and nothing else changes

```
enabled
&& !paused
&& secondsSinceMidnight(now) ∈ [target_time, target_time + 5)
&& (days_mask >> localWeekday(now)) & 1
&& last_fired_yday != localDayOfYear(now)
```

`localWeekday` and `localDayOfYear` both read from the same `localParts(epoch, tz)` (`LocalTime.cpp:58`), so at the second 02:30 of a fall-back night they agree on which day it is by construction. The existing spring-forward and fall-back scenarios are unaffected.

### 4. Pause and resume do not touch `last_fired_yday`

Two cases decide it:

```
  07:00 fires ─── 08:00 pause ─── 09:00 resume        must NOT fire again today
                                  last_fired_yday == today  ✓ suppressed

  paused for a week ─── 06:59 resume ─── 07:00        must fire
                                  last_fired_yday == old day ✓ fires
```

Leaving the record alone gives both for free. Resetting it on resume would break the first case. This is captured as a spec scenario so it is not "tidied up" later.

### 5. Update keeps the slot and its paused state; it does reset `last_fired_yday`

`setSchedule(index, …)` on an occupied schedule slot preserves `paused` and rewrites everything else, including `last_fired_yday = SCHEDULE_NEVER_FIRED`. Rationale:

- Preserving `paused`: Edit and Resume are distinct controls on the card. An edit that silently re-arms is the kind of thing that turns lights on at 07:00 while the user is away.
- Resetting `last_fired_yday`: the user just chose a time; if it is later today they expect it to fire today. This matches the existing behaviour of `setSchedule` (`TimerScheduler.cpp:194`).

If the slot held a countdown, or was empty, `paused` starts as `false`.

### 6. Persistence is additive with behaviour-preserving defaults

New keys `timer_%u_paused` (14 chars) and `timer_%u_days` (12 chars), both within the 15-character NVS key limit. Load with `getBool(key, false)` and `getUChar(key, SCHEDULE_EVERY_DAY)`. A schedule written by the previous firmware has neither key and loads as *armed, every day*, which is exactly how it behaved. No migration block, no write-back on first boot, nothing to roll back: older firmware simply ignores the two extra keys.

### 7. HTTP surface follows the existing "JSON body with index" style

```
POST /api/timers/schedule  {index?, hour, minute, action?, preset_index?, days?}
                          days: 1..127, default 127; 0 or >127 → 400
                          index of an occupied schedule → in-place update (decision 5)

POST /api/timers/pause   {index, paused}
                          400 if index out of range, slot empty, or slot is a countdown
                          200 {"success":true} otherwise; no-op if already in that state

GET  /api/timers         timers[i] gains  "paused": bool, "days_mask": 0..127
```

A `PATCH /api/timers` with partial fields was considered; the codebase has no PATCH handler and the explicit endpoint is easier to read from the page script. The pause endpoint persists via `saveTimersConfig` like every other mutation.

### 8. `getRemainingSeconds` scans ahead up to seven days

For a schedule: start with today's offset to `target_time` (or tomorrow's if already past), then step forward one day at a time until `(days_mask >> wday) & 1` is set, at most seven steps. It counts wall-clock days, so as today it is an hour out across a DST transition; the field only drives a UI hint. While paused it returns 0 and the UI does not display it.

### 9. Timers page: edit mode is a state of the existing form

```
  ┌ Schedule ─────────────────────────────────────────────────┐
  │ Schedule Time  [07:00]                                    │
  │ Repeat      (M)(T)(W)(T)(F)( S)( S)   ← toggle buttons │
  │ Action      [Load preset ▾]  Preset [Morning ▾]        │
  │ [ Add Schedule ]                                          │   create mode
  │ [ Update Schedule ]  cancel edit                          │   edit mode (index N held in JS)
  └────────────────────────────────────────────────────────┘

  ┌ Active Timers ─────────────────────────────────────────┐
  │▌SCHEDULE   07:00   Load "Morning"          [●━] Edit  ×   │
  │▌        ○●●●●●○                                        │
  │▌        M T W T F S S                                  │
  ├────────────────────────────────────────────────────────┤
  │▌PAUSED  07:00   Load "Morning"          [━○] Edit  ×   │  dimmed
  │▌        ○◌◌◌◌◌○                                        │
  │▌        M T W T F S S                                  │
  └────────────────────────────────────────────────────────┘
```

- **Day picker** is seven `<button type="button" aria-pressed>` elements, not checkboxes. Seven labelled checkboxes in a row would violate the touch-target and label-alignment rules in `web-ui-controls` and look nothing like the dots on the card. The picker stages state until Set/Update, so it is deliberately *not* a toggle switch either; the `web-ui-controls` delta adds it as its own control type.
- **Armed/paused** on the card is a toggle switch, because it acts immediately: that is exactly the existing "immediate effect → toggle switch" requirement. A failed request restores the previous position, as the touch-control toggle does today.
- **Edit** copies `target_time`, `action`, `preset_index` and `days_mask` into the form, records the index, and swaps the submit label. Cancel-edit clears the index and restores defaults. After a successful update the form returns to create mode.
- **Dots** are seven `<span>`s with a `data-on` attribute driven by the mask; the letter row beneath is static. Both rows are Monday-first, mapped from the Sunday-first mask in one small helper shared with the picker.
- The existing `activeTimers` filter already keeps every `alarm_daily` entry, so paused schedules remain listed without change.
- Styles for the day buttons and dots go in `common.css` (no inline `style` for layout, matching the boolean-control rule).

## Risks / Trade-offs

- **Scheduler logic is not host-tested.** `TimerScheduler.cpp` is outside the native build filter, so the mask and pause branches are verified on device. → Keep the new logic to two boolean terms in `checkTimers` and put the only non-trivial computation, `localWeekday`, in `LocalTime` where it *is* tested, including a fall-back-night case.
- **Sunday-first mask vs Monday-first UI.** A one-off index error would light the wrong dot or fire on the wrong day. → One shared JS helper for both directions, and a spec scenario that a Mon–Fri mask is `0x3E`.
- **Edit mode left half-done.** User clicks Edit, then navigates away or the fetch refreshes the list. → Edit state lives in a single JS variable; the list re-render does not touch the form; cancel-edit is always visible in edit mode.
- **Toggle switch on a card that also re-renders every second.** The countdown tick re-renders `timerList.innerHTML`, which would reset a toggle mid-click. → Re-render only on data change or when a countdown is present; schedule-only lists render once per fetch.
- **Response size.** Two more fields per slot in `GET /api/timers` add roughly 30 bytes per timer. → Well within the existing document size.

### 10. Twelve slots, and the `enabled` key shrinks to fit

`MAX_TIMERS` goes from 4 to 12. NVS caps a key at 15 characters and `timer_10_enabled` is 16, so the enabled key becomes `timer_%u_en`. Every other suffix (`type`, `action`, `preset`, `target`, `dur`, `lfd`, `paused`, `days`) is six characters or fewer and already fits. Load reads `timer_%u_en` when present and falls back to `timer_%u_enabled`; save writes the short key and removes the long one. Slots 4–11 have no keys on an upgraded device and load as empty. The `GET /api/timers` document grows to twelve entries, roughly 2.5 KB serialised, still comfortably inside the existing response handling.

## Migration Plan

None required beyond the key fallback above. Flash the new firmware; existing schedules load as armed, every day. Rolling back to the previous firmware leaves two unread NVS keys per slot, which are harmless.

## Open Questions

- None blocking. Whether the paused card should also show "next fires …" text was considered and dropped: the dots plus the toggle already say everything.
