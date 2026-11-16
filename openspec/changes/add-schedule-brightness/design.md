## Context

See `proposal.md` — Why. The relevant current state:

- `Config::TimerEntry` (`src/Config.h:134-155`) holds one timer slot: `enabled`, `type`, `action`, `preset_index`, `target_time`, `duration_seconds`, `last_fired_yday`, `paused`, `days_mask`. `Config::TimersConfig` (`Config.h:160-171`) holds twelve of them plus the timezone.
- `ConfigManager::loadTimersConfig()`/`saveTimersConfig()` (`src/Config.cpp:396-541`) persist one NVS key per field per slot, e.g. `timer_%u_days`, `timer_%u_paused`. NVS caps a key at 15 characters; the load defaults are chosen so keys written by older firmware reproduce their behaviour.
- `TimerScheduler::checkTimers()` runs on the Network task. When a schedule fires it calls `executeTimer()`, which calls `showController.queueShowChange("Solid", …)` for `TURN_OFF` or `showController.queuePresetLoad(...)` for `LOAD_PRESET` (`TimerScheduler.cpp:122-151`). Both enqueue a `ShowCommand` consumed by the LED task.
- Brightness is global. `ShowController::queueBrightnessChange(uint8_t)` (`ShowController.cpp:95`) enqueues `SET_BRIGHTNESS`, which stores the value and persists it to `DeviceConfig` (`ShowController.cpp:147-159`). The control page changes brightness through `POST /api/brightness` → the same method.
- `POST /api/timers/schedule` parses `hour`, `minute`, `index?`, `action?`, `preset_index?`, `days?` and calls `setSchedule()` (`WebServerManager.cpp:884-963`). `GET /api/timers` serialises each occupied slot (`WebServerManager.cpp:771-799`). `setSchedule()` treats a request naming an occupied schedule slot as an in-place update that replaces the settings and keeps the paused state (`TimerScheduler.cpp:176-219`).
- The timers page (`data/timers.html`) is embedded as a generated gzip blob (`src/generated/timers_gz.h`) built from `data/timers.html` by `scripts/compress_web.py`. The schedule form is create/edit-in-one with `editingIndex` in JS.

`TimerScheduler.cpp` is not in the native build filter (`platformio.ini:14`), so its new branches are verified on device; `Config` structs and `LocalTime` are host-tested.

## Goals / Non-Goals

**Goals:**
- A schedule can set the global brightness when it fires, independently of its action.
- A schedule with no brightness set leaves brightness exactly as it was.
- Zero is a usable brightness and is distinct from "not set".
- The value survives a reboot and round-trips through the HTTP API and the timers page.
- Existing schedules behave identically after the update, with no migration step.

**Non-Goals:**
- A per-show or per-preset brightness. Brightness stays the single global setting it is today.
- Brightness on countdown timers. Issue #23 is about schedules.
- Ramp/fade brightness over time. A schedule sets an absolute value.
- Restoring the previous brightness after a show ends.

## Decisions

### 1. `int16_t brightness = -1` with -1 as "not set"

All 256 values of a `uint8_t` are valid brightnesses, so no in-range sentinel exists. Two options:

| | A. `int16_t brightness = -1` | B. `uint8_t brightness` + `bool has_brightness` |
|---|---|---|
| Storage | one field | two fields |
| NVS | one key | one key (with the flag folded in) |
| Reads as | `brightness >= 0` | `has_brightness` |
| Range | -1…255, only -1 is special | 0…255 plus a flag |

A is chosen: one field, one key, and a single `brightness >= 0` test at every use site. `int16_t` (not `int8_t`/`uint8_t`) is required so -1 stays representable. The value is clamped to 0–255 when parsed.

### 2. The value is applied through the existing brightness command, after the action

`executeTimer()` calls `showController.queueBrightnessChange(static_cast<uint8_t>(timer.brightness))` when `timer.brightness >= 0`. This is the same path the control page uses, so it updates the rendered brightness and persists `DeviceConfig.brightness` in one step, and needs no new `ShowCommand` type. It is called after the action is queued so the FIFO queue applies the show/preset first and the brightness second; in practice order does not matter because `SET_SHOW`/`LOAD_PRESET` do not touch brightness, but "action then brightness" reads as the intended result.

Applying the brightness for **any** action (not just `LOAD_PRESET`) is deliberate: the brightness is a property of the schedule, and a `TURN_OFF` schedule that carries one still leaves the device's stored brightness at that value for the next time the lights are turned on. A user who does not want that simply leaves the schedule's brightness unset. This is captured as a spec scenario so it is not "tightened" to preset-only later.

### 3. HTTP: omitted/`null` clears; out-of-range is rejected

```
POST /api/timers/schedule  {index?, hour, minute, action?, preset_index?, days?, brightness?}
                          brightness: 0..255 when present → set
                                      absent or null     → no brightness (clear on update)
                                      otherwise          → 400 {"success":false,"error":"Invalid brightness"}
GET  /api/timers         timers[i] (schedule) gains  "brightness": 0..255 | null
```

`setSchedule()` already replaces the whole schedule on an in-place update (decision 5 of the weekday/pause change), so "absent clears" is the natural, consistent reading: the request is the new schedule in full. An explicit clear sentinel was considered and rejected — it would be a third state on the wire for no gain, since the page simply omits the field when the checkbox is unchecked. `null` is accepted as equivalent to absent because ArduinoJson's `|` default makes that the path of least resistance. Negative and >255 values are rejected rather than clamped, matching how `days` is validated; the page cannot produce them, but a script can.

### 4. Persistence: one additive key, `timer_%u_bright`

`timer_11_bright` is 15 characters, exactly the NVS key limit (`timer_11_paused` already sits at 15). Load with `prefs.getShort(key, -1)` and save with `prefs.putShort(...)`. A schedule written by the previous firmware has no key and loads as -1, i.e. no brightness, which reproduces its behaviour exactly. No migration block and no write-back: older firmware simply ignores the extra key, so rollback is harmless.

### 5. `setCountdown()` clears the brightness; `setSchedule()` sets it explicitly

A slot can be reused: a schedule with a brightness is later replaced by a countdown, or vice versa. `setCountdown()` does not currently reset every field, so it will set `timer.brightness = -1` to avoid a countdown inheriting a stale value it will never use or display. `setSchedule()` sets `timer.brightness` from its new parameter (the -1 sentinel for "unset") as part of the full replace. `cancelTimer()` already resets the whole entry to defaults, which now includes `brightness = -1`.

### 6. The timers page stages the value behind a checkbox

Per the existing `web-ui-controls` rule, a boolean whose change acts immediately is a toggle switch and one that stages state until a button is pressed is a checkbox. The schedule form's action does not reach the device until "Add/Update Schedule", so "Set brightness" is a checkbox, matching "Open network (no password)" and "Gradient mode". A numeric `input[type="number"]` with `min=0`, `max=255` holds the value and is disabled while the checkbox is unchecked, mirroring how "Open network" disables and clears the password field. The form sends `brightness` only when checked, so unchecking is how a user clears an existing value on Edit.

The card indicator is plain text next to the existing action line — e.g. `Brightness 64` — shown only when `timer.brightness !== null && timer.brightness !== undefined`. No new component is needed.

### 7. The scheduler branch is a single `if`

```
if (timer.brightness >= 0 && showController.queueBrightnessChange(static_cast<uint8_t>(timer.brightness))) {
    ESP_LOGI(TAG, "Schedule brightness -> %d", timer.brightness);
}
```

The only non-trivial part (parsing, range check) lives in the HTTP handler, which is observable and exercises the same validation as `days`. Keeping `checkTimers`/`executeTimer` to one guarded call minimizes the untestable surface in `TimerScheduler.cpp`.

## Risks / Trade-offs

- **`TimerScheduler.cpp` is not host-tested.** → The new scheduler logic is a single `brightness >= 0` guard around one existing call; the parsing and range logic is in `WebServerManager.cpp`'s handler and verified with `curl` on device. The sentinel and default live in `Config`, which is host-visible.
- **A `TURN_OFF` schedule that carries a brightness changes the stored brightness even though the LEDs are off.** → Intentional (decision 2) and surfaced as a spec scenario; the user opts in by setting a brightness on such a schedule.
- **Clearing on omission can surprise an API client that sends a partial update.** → Already the behaviour for every other field (an in-place update replaces the whole schedule), so it introduces no new inconsistency; documented in the spec and design.
- **Brightness changed by a schedule moves the control page's slider on its next poll.** → Correct: it is one global value. The control page already reflects `currentStatus.brightness`.
- **NVS key budget.** → One 6-character suffix per slot, still exactly at the 15-character limit for index 11; no other key changes.

## Migration Plan

None required. Flash the new firmware; existing schedules load with no brightness and behave as before. Rollback leaves one unread NVS key per slot, which is harmless.

## Open Questions

- None blocking. Whether brightness should also be offered on countdown timers is out of scope per issue #23; it can be a follow-up if the UI makes countdowns the more natural place for it.
