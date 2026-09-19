# Spec Deltas

## ADDED Requirements

### Requirement: The timers page shows a single Countdown timer section with preset shortcuts

The timers page SHALL present a countdown timer control as a single section whose heading reads "Countdown timer". The section SHALL contain a row of four preset duration shortcuts above the duration input, labelled "15 min", "30 min", "1 hour" and "2 hours", and SHALL contain a form for arbitrary durations and actions below the preset row. The form SHALL expose a duration input in minutes, an action select with options "Turn Off LEDs" and "Load preset", a conditional preset selector shown only when the action is "Load preset", and a submit button labelled "Set Countdown Timer". The submit button SHALL be the only path that creates a countdown timer. No separate "Quick Off Timer" section SHALL be present.

#### Scenario: Page layout
- **WHEN** the timers page is displayed
- **THEN** the page shows one Countdown timer section, not two
- **THEN** the section's heading reads "Countdown timer"
- **THEN** the section contains a preset row of four buttons and a form, in that order

#### Scenario: Preset buttons styled as secondary shortcuts
- **WHEN** the preset row is rendered
- **THEN** each preset button uses the secondary small button styling and reads as visually subordinate to the "Set Countdown Timer" submit button

### Requirement: A preset click pre-fills the countdown form and does not submit

Activating a preset button SHALL write the corresponding number of minutes into the duration input, SHALL set the action select to "Turn Off LEDs", SHALL hide the preset selector group, and SHALL focus the duration input. Activating a preset button SHALL NOT send a request to the device.

#### Scenario: Pre-fill duration in minutes
- **WHEN** the user activates the "30 min" preset
- **THEN** the duration input shows `30`
- **THEN** no POST is sent to `/api/timers/countdown`

#### Scenario: Pre-fill resets action to off
- **WHEN** the user had previously set the action to "Load preset" with a selected preset and then activates any preset button
- **THEN** the action select shows "Turn Off LEDs"
- **THEN** the preset selector group is hidden

#### Scenario: Focus moves to the duration input
- **WHEN** the user activates any preset button
- **THEN** the duration input receives keyboard focus so the value can be edited immediately

### Requirement: The Countdown timer section's submit button creates the timer

Submitting the Countdown timer section's form, regardless of whether the duration was typed in or selected via a preset, SHALL create a single countdown timer via `POST /api/timers/countdown` using the form's current duration, action and preset values. The active timers list SHALL refresh after a successful submission.

#### Scenario: Submit after preset click
- **WHEN** the user activates the "15 min" preset and then clicks "Set Countdown Timer"
- **THEN** a countdown with duration 15 minutes and action "Turn Off LEDs" is created
- **THEN** the active timers list shows the new countdown

#### Scenario: Submit with edited preset duration
- **WHEN** the user activates the "30 min" preset, changes the duration input to 45, and clicks "Set Countdown Timer"
- **THEN** a countdown with duration 45 minutes and action "Turn Off LEDs" is created
