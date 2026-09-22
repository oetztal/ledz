## Why

The control page (`/`) puts four things in the top status bar: Current Show, Brightness, an "Off" button and a one-line "next timer" summary. When a timer is configured, the timer summary appears to the right of the Off button, so the button — the page's primary kill switch — drifts out of the top-right corner and stops being a stable landmark. The button also shows the plain text "Off", which competes visually with the status values and reads more like a passive label than an action.

## What Changes

- Detach the Off button from the status bar and place it as a circular floating action in the top-right of `.content`, with an inline power-symbol SVG icon and an accessible label. Behaviour (POST Solid-black, persists, disables while in flight) is unchanged.
- Remove the timer summary cell from the status bar. Move the next-timer information into a dedicated banner between the status bar and the Brightness control, styled like the timezone band on `/timers` and visually echoing an Active Timers card from `/timers`. The banner shows the next active timer (countdown first, then soonest schedule) with type badge, mono time, action description and day dots. It is fully hidden when no timer is enabled.
- Refactor `updateTimerDisplay()` in `data/control.html` so it renders into the banner DOM rather than into the removed status-bar cell. No new HTTP calls; existing `GET /api/timers` and one-second poll continue to drive the banner.
- Keep the timer's "Manage" affordance as a single link to `/timers` from the banner. Pause, edit and cancel controls stay on `/timers` only — accidental cancellation from the home page is not desired.

## Capabilities

### New Capabilities

- `control-page-next-timer`: A status banner on `/` that shows the next active timer (countdown, then soonest schedule) with type badge, mono time, action description and day dots, fully hidden when no timer is enabled. The banner includes a single "Manage →" link to `/timers` and carries no other controls.

### Modified Capabilities

- `led-off-action`: The Off button is rendered as an icon-only circular floating action in the top-right of `.content` instead of as a text-labelled cell inside `.status-bar`. Its behaviour (single payload, persistence, in-flight disabling, reversal by selecting any other show) is unchanged.

## Impact

- `data/control.html`: status bar loses its third and fourth items; new floating Off button added as a direct child of `.content`; new `#timerBanner` block added between `.status-bar` and the Brightness control group; `updateTimerDisplay()` rewritten to populate the banner DOM; `renderDayDots`-equivalent helper added locally (same algorithm as `data/timers.html:225-230`).
- `data/common.css`: new `.off-button` rules (position, size, drop shadow, hover/active/disabled). No existing rules removed.
- No C++ changes. No new endpoints. No NVS schema change. The existing `POST /api/show` Solid-black payload and `GET /api/timers` shape are reused.
- After editing, `scripts/compress_web.py` (PlatformIO pre-build hook) regenerates `src/generated/control_gz.h` from `data/control.html` automatically.
- Behavioural coupling: a scheduled `TURN_OFF` timer and the Off button continue to converge on the same internal state (`Solid` with `colors=[[0,0,0]]`).
