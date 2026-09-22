# control-page-next-timer Specification

## Purpose
The control page surfaces the timer that will fire next — whether it is a countdown or a schedule — as a compact banner above the brightness controls, so the user can see upcoming automatic changes without opening the timers page.
## Requirements
### Requirement: Control page renders a next-timer banner when at least one timer is enabled

The control page SHALL render a banner element between the status bar and the Brightness control group whenever at least one timer with `enabled=true` exists on the device. The banner SHALL be fully hidden (no space taken in the layout) when no timer is enabled.

#### Scenario: Banner appears when a timer exists
- **WHEN** `GET /api/timers` reports at least one entry with `enabled=true`
- **THEN** the banner is visible between the status bar and the Brightness control group

#### Scenario: Banner is hidden when no timer exists
- **WHEN** `GET /api/timers` reports zero entries with `enabled=true`
- **THEN** the banner's `display` is `none` and the page flow continues uninterrupted from the status bar to the Brightness control group

### Requirement: Banner shows the next timer that will actually fire

The banner SHALL display, at most, one timer: the one that will fire next across all enabled timers, regardless of whether it is a countdown or a schedule. Paused schedules SHALL be sorted to the end (they do not actually fire), so an active timer is preferred whenever one exists. The slot index SHALL break ties. For countdowns, the next firing moment is `timersFetchedAt + remaining_seconds`; for schedules it is the next occurrence of `target_time` on or after the current local time.

#### Scenario: One countdown and one schedule exist
- **WHEN** `GET /api/timers` reports a countdown with remaining time 0:30 and a schedule for 22:00 (current local time is 14:00, so the schedule fires in 8 hours)
- **THEN** the banner displays the countdown
- **THEN** the banner does not show the schedule

#### Scenario: Two schedules exist
- **WHEN** `GET /api/timers` reports a schedule for 22:00 (later today) and a schedule for 07:00 (already past today's 07:00, so it fires tomorrow) and current local time is 14:00
- **THEN** the banner displays the 22:00 schedule (next firing is in 8 hours, not tomorrow's 07:00)

#### Scenario: Paused schedule is sorted after an active one
- **WHEN** `GET /api/timers` reports an active schedule for 22:00 and a paused schedule for 07:00, and current local time is 14:00
- **THEN** the banner displays the 22:00 schedule (active wins, even though the paused one's time-of-day is earlier)

#### Scenario: Only paused schedules exist
- **WHEN** `GET /api/timers` reports only paused schedules
- **THEN** the banner still displays one of them, with a `PAUSED` badge

### Requirement: Banner displays type badge, mono time, action description, and day dots

The banner SHALL contain: a type label badge reading `TIMER`, `SCHEDULE` or `PAUSED` according to the timer's type and paused state; a monospaced time string that reads the countdown remaining time (formatted as `H:MM:SS` or `M:SS`) for countdowns and the time of day (`HH:MM`) for schedules; an action description line that reads `Turn off` or `Load "<preset name>"` (or `Load preset #<n>` when the named preset is not available locally); for schedules with a weekday mask, a Monday-first row of seven day dots, filled for each selected weekday, with the day initials beneath.

#### Scenario: Countdown banner
- **WHEN** a countdown timer with action `TURN_OFF` and remaining time 14:23 is displayed in the banner
- **THEN** the badge reads `TIMER`
- **THEN** the mono time reads `14:23`
- **THEN** the action description reads `Turn off`
- **THEN** no day-dots row is shown (countdowns do not have a weekday mask)

#### Scenario: Armed schedule banner
- **WHEN** a schedule at 07:00 with weekday mask `0x3E` (Monday to Friday), action `LOAD_PRESET` for preset `Morning`, and paused=false is displayed in the banner
- **THEN** the badge reads `SCHEDULE`
- **THEN** the mono time reads `07:00`
- **THEN** the action description reads `Load "Morning"`
- **THEN** the day-dots row shows five filled dots over `M T W T F` and two empty dots over `S S`

#### Scenario: Paused schedule banner
- **WHEN** a schedule is displayed in the banner with `paused=true`
- **THEN** the badge reads `PAUSED`

### Requirement: Banner includes a Manage link to the timers page

The banner SHALL contain a single textual link reading `Manage →` (or equivalent affordance text indicating navigation) that navigates to `/timers`. The link SHALL be the only interactive control inside the banner other than the badge text.

#### Scenario: Manage link is present
- **WHEN** the banner is visible
- **THEN** a link to `/timers` is rendered inside the banner
- **THEN** activating the link navigates the browser to `/timers`

#### Scenario: No inline editing or cancellation
- **WHEN** the banner is visible
- **THEN** the banner does not contain pause, edit or cancel controls
- **THEN** the banner does not send any request to the device when interacted with apart from navigating to `/timers`

### Requirement: Banner updates as timers change

The banner SHALL re-render in response to the same signals that drive other status surfaces on the control page: a fresh `GET /api/timers` response and the one-second interval tick used by `updateStatus()`.

#### Scenario: Fresh fetch updates the banner
- **WHEN** a new countdown timer is created via `POST /api/timers/countdown`
- **THEN** the next successful `GET /api/timers` causes the banner to display the new countdown
- **THEN** the banner remains in sync with the underlying timer for as long as it remains enabled

#### Scenario: Countdown ticks every second
- **WHEN** a countdown timer is displayed in the banner
- **THEN** the mono time decreases by one second each interval tick until the countdown expires
- **WHEN** the countdown's remaining time reaches zero and the backend removes the timer
- **THEN** the banner hides on the next render

### Requirement: Banner visual treatment matches the timers page

The banner SHALL be styled with the same purple gradient, `padding: 20px`, and `border-radius: 12px` used by the timezone band on the timers page and the page header, so it reads as part of the same visual family. The type badge SHALL be a small uppercase chip on a translucent background. Day dots SHALL be filled or hollow circles; the row SHALL be Monday-first with `M T W T F S S` initials beneath.

#### Scenario: Visual continuity
- **WHEN** the banner is rendered on the control page
- **THEN** its background matches the gradient of the page header
- **THEN** its border-radius matches the timezone band's radius on the timers page
