## 1. Update timers.html markup

- [x] 1.1 Delete the standalone "Quick Off Timer" section (its `<div class="section">`, heading, description and four buttons).
- [x] 1.2 In the remaining countdown section, rename the heading from "Custom Countdown Timer" to "Countdown timer".
- [x] 1.3 Replace the section description paragraph with "Pick a preset or set a custom duration and action."
- [x] 1.4 Inside the section, add a `.preset-row` strip directly below the description and above the first `.form-group`, containing four `<button class="btn-secondary btn-small">` elements calling `applyCountdownPreset(15)`, `applyCountdownPreset(30)`, `applyCountdownPreset(60)` and `applyCountdownPreset(120)` with labels "15 min", "30 min", "1 hour" and "2 hours".

## 2. Update timers.html JavaScript

- [x] 2.1 Replace the body of `setQuickTimer(seconds)` with `applyCountdownPreset(minutes)` whose body sets `countdownMinutes.value = minutes`, sets `countdownAction.value = "off"`, sets `countdownPresetGroup.style.display = "none"`, then calls `countdownMinutes.focus()` and `countdownMinutes.select()`.
- [x] 2.2 Leave `setCustomCountdown` unchanged: it remains the only path that POSTs to `/api/timers/countdown`.

## 3. Verify

- [x] 3.1 Open `data/timers.html` in a browser against a running device, click each preset, edit or leave the duration, click "Set Countdown Timer", and confirm a single countdown appears in the Active Timers list with the expected duration and action.
- [x] 3.2 Verify that clicking a preset after having previously set action to "Load preset" with a selected preset resets the form to action "Turn Off LEDs" and hides the preset selector.
- [x] 3.3 Verify that `test/test_localtime` still passes (`pio test -e native -f test_localtime`) — it only asserts the file is loadable but guards against the file having been corrupted.
