## Why

The timers page currently shows two adjacent sections that do the same thing: "Quick Off Timer" is a row of preset buttons (15 min / 30 min / 1 h / 2 h) that POST a countdown directly, and "Custom Countdown Timer" is a form for arbitrary durations and actions that POSTs the same endpoint. Two sections for one primitive makes the page longer than it needs to be and forces a name like "Custom Countdown Timer" whose only purpose is to contrast with its neighbour. Merging the presets into the countdown section lets the section drop the "Custom" qualifier and shrinks the page from four sections to three.

## What Changes

- Rename the "Custom Countdown Timer" section to "Countdown timer" and replace its description with one that names both the preset and form affordances.
- Delete the standalone "Quick Off Timer" section.
- Inside the Countdown timer section, add a row of four secondary-style preset buttons (15 min, 30 min, 1 hour, 2 hours) above the existing form fields.
- Change the preset buttons from one-click submit to pre-fill: each click writes the duration (in minutes) into the form's `countdownMinutes` input, sets the action to "Turn Off LEDs", hides the preset selector and focuses the duration input. The form's existing "Set Countdown Timer" submit button remains the only path that actually creates a timer.
- Rename the `setQuickTimer(seconds)` JS helper to `applyCountdownPreset(seconds)` so the name matches its new pre-fill semantics.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `timer-scheduling`: The timers page currently shows the countdown primitives as two sections ("Quick Off Timer" and "Custom Countdown Timer"). After the merge, the page has a single "Countdown timer" section that contains both a row of preset shortcuts and a form. The HTTP surface, payload shape and scheduling semantics are unchanged, but the requirement that describes the timers page's UI for countdowns gains new scenarios covering the merged layout and the pre-fill behaviour of the preset row.

## Impact

- `data/timers.html` — section markup, description copy and the `setQuickTimer` / `applyCountdownPreset` JS helper.
- No backend, endpoint, payload, NVS schema or firmware changes.
- No CSS changes; the new preset row reuses `.btn-secondary.btn-small` already defined in `data/common.css` for the schedule card's Edit button.
- No test impact: `test/test_localtime` only asserts that the file is loadable and does not parse the JS.
