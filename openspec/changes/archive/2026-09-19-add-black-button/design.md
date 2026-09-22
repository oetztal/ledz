## Context

The control page (`data/control.html`) currently exposes the show dropdown, per-show parameter panels, brightness, and a status bar showing the current show. There is no direct "off" control. The timer's `TURN_OFF` action (`src/TimerScheduler.cpp:139`) already implements an LED-off behaviour by switching to the `Solid` show with `colors=[[0,0,0]]`, and that state is persisted to NVS through the existing `SaveShowConfig` path (`src/ShowController.cpp:147,237`). The web button must converge on the same internal state so that a manual off and a scheduled off are indistinguishable to the device.

## Goals / Non-Goals

**Goals:**
- One-click off from the control page, no parameters, no confirmation dialog.
- State survives reboot (no separate "remember previous show" logic).
- Zero new C++ code: the existing `POST /api/show` endpoint accepts the Solid-black payload that the timer already uses.
- The control page's status bar and show dropdown update to reflect the new state without a full page reload.

**Non-Goals:**
- A toggle that remembers and restores the previous show.
- A hardware (touch controller) off gesture — out of scope; the touch controller is independent.
- Changing the persistence model — the show name and params are already what gets saved.
- Changing the `Solid` show itself or how it renders all-black.

## Decisions

### Reuse `POST /api/show` instead of a new endpoint

The Solid show already accepts `colors: [[0,0,0]]` (`src/ShowFactory.cpp:24`) and `ShowController::queueShowChange` persists it. A new endpoint would be a thin wrapper that does the same thing. Reusing `/api/show` means:
- Zero backend changes (no build, no firmware risk).
- The same code path that the timer's `TURN_OFF` uses, so behaviour is guaranteed identical.
- The `updateStatus()` poll (10 s, `data/control.html:1209`) already re-renders the dropdown and status bar when the show changes, so the UI updates itself.

**Alternatives considered:**
- New `POST /api/off` handler: rejected — pure duplication of `/api/show` with a fixed payload.
- `ShowController::clearStrip()` plus a separate "remember on next show" path: rejected — `clearStrip()` is overwritten by the next show tick, so it cannot persist; persisting "off" without changing the show would require a new state field and would not survive reboot.

### Placement: status bar, third item

The button sits in `.status-bar` alongside the existing "Current Show" and "Brightness" items. Brightness is the other power-level control on the page, so grouping "Off" with it is semantically tidy. Putting it in the header would compete with the device id / OTA badge area; a standalone row between status and brightness would clutter the page without functional justification.

### Visual: `.btn-danger`, label "Off"

The existing `.btn-danger` class (`data/common.css:135`) is the established "destructive-ish" style. Red fits the "stop the lights" intent. A full-width `.btn-danger` button as a third `.status-item` is consistent with how the brightness slider sits full-width further down the page. Label "Off" reads cleaner than "Black" (the show underneath is named `Solid`, the colour is black — "Off" describes the user intent, not the implementation).

**Alternatives considered:**
- `.btn-secondary` (gray): quieter but ambiguous — could read as a "cancel" button.
- `.small-button` (chip): inconsistent with full-width controls elsewhere; would feel like another preset.
- Icon only (`●`, `🌙`): loses meaning for first-time users; no precedent in this UI.

### Reverse path: pick any show from the dropdown

The colour-ranges parameter panel becomes visible after the off action because `updateParameterVisibility("Solid")` shows it. This is a free "wake up" affordance — the user can pick a non-black colour and click "Apply Pattern", or pick any other show from the dropdown. No dedicated "turn back on" button is added.

## Risks / Trade-offs

- **Confusion from "Solid" appearing as the show name after pressing Off** → Acceptable. The dropdown is truthful (the show really is Solid), and the colour picker doubles as the obvious next step.
- **Concurrent scheduled `TURN_OFF` / schedule-fire / `LOAD_PRESET` overrides the manual off** → Desired behaviour. Schedules and presets are explicit user intent that should win over a one-click off. No mitigation needed.
- **No undo** → The user can pick another show at any time. A dedicated "restore previous show" button would need server-side state and was explicitly rejected as a non-goal.
- **compress_web.py regeneration step** → The new HTML must be re-gzipped. `scripts/compress_web.py` is part of the standard build flow and is documented in `AGENTS.md` and the source comment at `src/WebServerManager.cpp:81`. The task list calls this out.
- **NVS write on every press** → Each press triggers `SaveShowConfig` via the existing path. This is the same cost as any other show change; not a regression.

## Migration Plan

None. The change is additive on the web side only and does not alter any contract. The `ledz` device firmware version is unchanged; the change ships with whatever release it's bundled into.

## Open Questions

- Exact visual treatment inside the status bar (compact vs full-width `.btn-danger`, icon, padding tweaks) — to be settled during implementation; the design above is the default and is cheap to adjust.
- Whether to also surface an "Off" button on the settings or about pages — out of scope per the non-goals, but trivially addable later if requested.
