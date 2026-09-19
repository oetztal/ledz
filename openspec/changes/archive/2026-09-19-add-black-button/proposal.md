## Why

The web control page lets the user pick a show, tune its parameters, and adjust brightness, but offers no direct way to turn the LEDs off. To stop the strip a user must either schedule a turn-off timer, edit a preset to all-black and load it, or rely on the touch controller. The timer-scheduling capability already supports a `TURN_OFF` action by switching to the Solid show with an all-black colour range — that same mechanism is the natural fit for a one-click off button, with state persisting across reboots.

## What Changes

- Add an "Off" button to the control page (`data/control.html`) that turns the LED strip black by switching the active show to `Solid` with a single black colour (`colors=[[0,0,0]]`).
- The action uses the existing `POST /api/show` endpoint; the show is persisted to NVS in the same way as any other show change, so the device boots into black after a power cycle.
- The control page's status bar and show dropdown reflect `Solid` after the button is pressed (the colour-ranges parameter panel becomes visible, providing an immediate "wake up" affordance).
- The user restores the strip by picking a show from the dropdown (or any non-black colour in the colour-ranges panel).

## Capabilities

### New Capabilities

- `led-off-action`: A user-initiated action on the control page that switches the active show to `Solid` with all-black colours, persists that choice across reboots, and can be reversed by selecting any other show.

### Modified Capabilities

None. The existing `web-ui-controls` spec governs form-control styling (checkboxes, toggles, weekday pickers) — a one-shot action button does not fall under that contract. No server contract is changed: `POST /api/show` already accepts the Solid-black payload, used today by the timer's `TURN_OFF` action.

## Impact

- `data/control.html`: one button element added (placement TBD), one JS handler that posts the Solid-black payload and triggers `updateStatus()`.
- `data/common.css`: possibly a small style rule if a new visual treatment is desired; existing `.btn-danger` / `.btn-secondary` / `.small-button` classes are available without modification.
- No C++ changes; no new endpoint; no NVS schema change.
- Behavioural coupling: a scheduled `TURN_OFF` timer and the new button converge on the same internal state, which is desirable.