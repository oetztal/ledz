## ADDED Requirements

### Requirement: A weekday picker is a group of seven toggle buttons
A control for choosing a set of weekdays SHALL be rendered as a row of seven `button` elements of `type="button"`, one per day, each carrying `aria-pressed` reflecting its selected state, labelled with the day's initial and carrying an accessible full day name. The row SHALL be ordered Monday first. The picker SHALL stage its state and SHALL NOT send a request until the enclosing form's submit button is pressed. Each button SHALL meet the 44-pixel minimum touch target and SHALL show a visible focus indicator when focused by keyboard. The picker SHALL NOT rely on inline `style` attributes for its layout.

#### Scenario: Toggling a day
- **WHEN** the user activates the "W" button in the schedule form's weekday picker
- **THEN** its `aria-pressed` state flips and its appearance changes to match
- **THEN** no request is sent to the device

#### Scenario: Keyboard use
- **WHEN** the user tabs to a weekday button and presses Space or Enter
- **THEN** the day toggles and a visible focus indicator is shown on that button

#### Scenario: Default selection
- **WHEN** the schedule form is displayed in create mode
- **THEN** all seven days are selected

### Requirement: A schedule card shows its weekdays as a row of seven dots
Each schedule entry in the Active Timers list SHALL show a Monday-first row of seven dots beneath its time, a filled dot for each selected weekday and a hollow dot for each unselected one, with the day initials shown beneath the dots. The dots SHALL be a read-only indicator.

#### Scenario: Weekday schedule
- **WHEN** a schedule with mask `0x3E` (Monday to Friday) is listed
- **THEN** the first five dots are filled and the last two are hollow
- **THEN** the initials beneath read M T W T F S S

#### Scenario: Every-day schedule
- **WHEN** a schedule with mask `0x7F` is listed
- **THEN** all seven dots are filled

### Requirement: Schedule cards expose pause, edit and cancel
Each schedule entry in the Active Timers list SHALL show a toggle switch for its armed state, an Edit button and a Cancel button. Because the armed state acts on the device immediately, it SHALL be a toggle switch per the existing rule for boolean controls, and a failed request SHALL restore its previous position. A paused schedule's card SHALL be visually dimmed and labelled `PAUSED`; an armed schedule's card SHALL be labelled `SCHEDULE`. Activating Edit SHALL load the schedule's time, action, preset and weekdays into the schedule form, switch the form's submit label to indicate an update, show a cancel-edit control, and SHALL NOT send a request. Submitting the form in edit mode SHALL update that schedule in place and return the form to create mode.

#### Scenario: Pausing from the card
- **WHEN** the user switches an armed schedule's toggle off
- **THEN** a pause request is sent immediately
- **THEN** on success the card dims and its label reads `PAUSED`

#### Scenario: Pause request fails
- **WHEN** the pause request fails
- **THEN** the toggle returns to its previous position and the card is unchanged

#### Scenario: Editing a schedule
- **WHEN** the user activates Edit on a schedule set for 07:00, Monday to Friday, loading preset "Morning"
- **THEN** the form shows 07:00, the five weekday buttons pressed, action "Load preset" and preset "Morning"
- **THEN** the submit button reads "Update Schedule" and a cancel-edit control is visible

#### Scenario: Cancelling an edit
- **WHEN** the user activates the cancel-edit control
- **THEN** the form returns to create mode with default values and no request is sent

#### Scenario: Submitting an edit
- **WHEN** the user changes the time to 08:00 and submits in edit mode
- **THEN** the request names the edited schedule's index
- **THEN** on success the list shows the schedule at 08:00 in its sorted position and the form returns to create mode

### Requirement: Timer entries are listed in clock order
The Active Timers list SHALL show countdown timers first, soonest to expire on top, followed by schedules ordered by their time of day. Slot index SHALL only break ties.

#### Scenario: Schedules sorted by time
- **WHEN** slot 0 holds a schedule for 22:00 and slot 1 holds a schedule for 07:00
- **THEN** the 07:00 schedule is listed above the 22:00 schedule
