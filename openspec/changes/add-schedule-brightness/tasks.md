## 1. Config: struct and NVS

- [ ] 1.1 `src/Config.h`: add `int16_t brightness = -1;` to `TimerEntry` with a comment stating that -1 means "no brightness set" and 0 is a valid brightness (design decision 1); confirm `TimerEntry` still default-constructs to "no brightness"
- [ ] 1.2 `src/Config.cpp` `loadTimersConfig`: inside the `if (enabled)` block read `timer_%u_bright` with `prefs.getShort(key, -1)` into `brightness`
- [ ] 1.3 `src/Config.cpp` `saveTimersConfig`: write `timer_%u_bright` with `putShort` for every occupied slot; confirm the key buffer still fits `timer_11_bright` (15 characters, the NVS limit)
- [ ] 1.4 Add a native assertion that a default `TimerEntry` has `brightness == -1` and that 0 is a distinct, storable value — either in `test/test_wifi_credentials` (which already includes `Config.h` through `support/WiFiCredentials.h`) or in a new `test/test_config` suite; `Config.h` is native-safe by design

## 2. TimerScheduler

- [ ] 2.1 `src/TimerScheduler.h`: `setSchedule` gains a trailing `int16_t brightness = -1` parameter (documented: -1 = unset, 0–255 = set); update the doc comment
- [ ] 2.2 `src/TimerScheduler.cpp` `setSchedule`: store `timer.brightness = brightness` as part of the full replace (always, so an in-place update without a value clears it) and log the value alongside the existing schedule log
- [ ] 2.3 `src/TimerScheduler.cpp` `setCountdown`: set `timer.brightness = -1` so a reused slot cannot inherit a previous schedule's value (design decision 5)
- [ ] 2.4 `src/TimerScheduler.cpp` `executeTimer`: apply the brightness only in the `LOAD_PRESET` case, after the preset has been queued — when `timer.brightness >= 0` call `showController.queueBrightnessChange(static_cast<uint8_t>(timer.brightness))` and log it. Leave the `TURN_OFF` case untouched so it never reads `timer.brightness` and a turn-off schedule never changes the global brightness (design decisions 2 and 7)
- [ ] 2.5 Confirm `cancelTimer`'s `timersConfig.timers[index] = Config::TimerEntry{}` now also clears `brightness`, and `pio run -e adafruit_qtpy_esp32s3_nopsram` compiles

## 3. HTTP API

- [ ] 3.1 `src/WebServerManager.cpp` `POST /api/timers/schedule`: parse optional `brightness` as `int` defaulting to -1 (absent or `null` → "not set"); reject a present value below 0 or above 255 with 400 `{"success":false,"error":"Invalid brightness"}` and change no slot; pass the value to `setSchedule`. Because `setSchedule` stores the parameter unconditionally, an in-place update without the field stores -1 and clears any previously stored brightness (reviewer point 2)
- [ ] 3.2 `src/WebServerManager.cpp` `GET /api/timers`: for an occupied `SCHEDULE`, add `brightness` as the integer when `timer.brightness >= 0`, otherwise `null`; do not add it for countdown slots
- [ ] 3.3 Manual `curl` pass against a device covering the "set and read over HTTP" scenarios: create with `brightness:64`, create without it (expect `null`), update with `brightness:200` (expect 200), update without it (expect `null` and confirm firing the updated schedule no longer changes brightness), `brightness:256` and `brightness:-1` (expect 400 and no change)

## 4. On-device firing behaviour

- [ ] 4.1 Set a "Load preset" schedule one minute ahead with a brightness, set the control-page brightness to something else, and confirm the device's brightness becomes the schedule's value when it fires and that the persisted brightness survives a reboot
- [ ] 4.2 Fire a schedule with no brightness and confirm the current brightness is unchanged
- [ ] 4.3 Confirm `brightness: 0` is applied as 0 and is not treated as "unset"
- [ ] 4.4 Create a schedule whose action is "Turn Off LEDs" with a brightness set (via the API, since the form hides the control) and confirm the LEDs turn off and the stored/global brightness is unchanged (design decision 2, reviewer point 1)
- [ ] 4.5 Boot a device with a schedule created by the previous firmware and confirm it fires without changing brightness (legacy default)

## 5. Timers page

- [ ] 5.1 `data/timers.html`: add a `.form-group` (e.g. `scheduleBrightnessGroup`) with a `<label for="scheduleBrightnessEnabled">Set brightness</label>` checkbox and a numeric `input[type="number"]` with `min="0" max="255"`; show the group only while the action is "Load preset", mirroring `schedulePresetGroup`; no inline `style` for layout
- [ ] 5.2 JS: toggle the numeric input's `disabled` from the checkbox (`change` handler), matching how "Open network" disables the password field; also show/hide the group from the action selector's `change` handler
- [ ] 5.3 JS: `setSchedule()` includes `brightness` from the numeric input only when the checkbox is checked (which can only happen for a "Load preset" schedule), and casts it to a number
- [ ] 5.4 JS: `startEdit()` shows and pre-fills the brightness control when the schedule's action is "Load preset" and `timer.brightness` is an integer, and keeps it hidden, unchecked and disabled otherwise (a turn-off schedule or no value); `cancelEdit()` restores the hidden, unchecked default
- [ ] 5.5 `data/timers.html` card render: append a `Brightness <n>` fragment to the card's detail line only when `timer.brightness !== null && timer.brightness !== undefined`
- [ ] 5.6 Regenerate `src/generated/timers_gz.h` via the normal build (`scripts/compress_web.py` pre-step) and note the size delta

## 6. Verification

- [ ] 6.1 Keyboard pass over the new checkbox and numeric input on the timers page; confirm label association and focus indicator
- [ ] 6.2 Round-trip: create a brightness schedule, Edit it and confirm the checkbox/value are pre-filled, uncheck and update, confirm the card indicator disappears and `GET /api/timers` reports `null`
- [ ] 6.3 `pio test -e native` and the embedded build both green; run `clang-format` on changed C/C++ files

## 7. Docs and specs

- [ ] 7.1 README "Timers & Schedules" feature line: mention that a schedule can set brightness
- [ ] 7.2 Run `scripts/validate-openspec.sh` (or `openspec validate add-schedule-brightness --strict`) and fix any findings before declaring the change ready
