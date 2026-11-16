## MODIFIED Requirements

### Requirement: Schedule cards expose pause, edit and cancel
Each schedule entry in the Active Timers list SHALL show a toggle switch for its armed state, an Edit button and a Cancel button. Because the armed state acts on the device immediately, it SHALL be a toggle switch per the existing rule for boolean controls, and a failed request SHALL restore its previous position. A paused schedule's card SHALL be visually dimmed and labelled `PAUSED`; an armed schedule's card SHALL be labelled `SCHEDULE`. A schedule that has a brightness value set SHALL show that value on its card; a schedule with no brightness value set SHALL show no brightness indicator. Activating Edit SHALL load the schedule's time, action, preset, weekdays and brightness into the schedule form, switch the form's submit label to indicate an update, show a cancel-edit control, and SHALL NOT send a request. Submitting the form in edit mode SHALL update that schedule in place and return the form to create mode.

#### Scenario: Pausing from the card
- **WHEN** the user switches an armed schedule's toggle off
- **THEN** a pause request is sent immediately
- **THEN** on success the card dims and its label reads `PAUSED`

#### Scenario: Pause request fails
- **WHEN** the pause request fails
- **THEN** the toggle returns to its previous position and the card is unchanged

#### Scenario: Editing a schedule
- **WHEN** the user activates Edit on a schedule set for 07:00, Monday to Friday, loading preset "Morning" with brightness 64
- **THEN** the form shows 07:00, the five weekday buttons pressed, action "Load preset", preset "Morning", and the brightness control checked at 64
- **THEN** the submit button reads "Update Schedule" and a cancel-edit control is visible

#### Scenario: Cancelling an edit
- **WHEN** the user activates the cancel-edit control
- **THEN** the form returns to create mode with default values and no request is sent

#### Scenario: Submitting an edit
- **WHEN** the user changes the time to 08:00 and submits in edit mode
- **THEN** the request names the edited schedule's index
- **THEN** on success the list shows the schedule at 08:00 in its sorted position and the form returns to create mode

#### Scenario: Card shows the brightness
- **WHEN** a schedule with brightness 64 is listed
- **THEN** its card indicates brightness 64

#### Scenario: Card without a brightness
- **WHEN** a schedule with no brightness value set is listed
- **THEN** its card shows no brightness indicator

## ADDED Requirements

### Requirement: The schedule form offers a staged brightness control
The schedule form SHALL provide a control for an optional brightness value consisting of a checkbox that stages whether a brightness is set and a numeric control that accepts a value from 0 to 255. The checkbox SHALL be a checkbox, not a toggle switch, because it stages state that is applied only when the form is submitted. The numeric control SHALL be enabled while the checkbox is checked and disabled while it is unchecked. The form SHALL include `brightness` in its request only when the checkbox is checked, using the numeric control's value. In create mode the checkbox SHALL be unchecked. The control SHALL NOT rely on inline `style` attributes for its layout.

#### Scenario: Checkbox stages the value
- **WHEN** the user checks "Set brightness" and enters 64
- **THEN** no request is sent until the form is submitted
- **THEN** the numeric input is enabled

#### Scenario: Unchecked omits the brightness
- **WHEN** the user submits the form with "Set brightness" unchecked
- **THEN** the request omits `brightness`

#### Scenario: Editing a schedule with a brightness
- **WHEN** the user activates Edit on a schedule whose brightness is 64
- **THEN** the "Set brightness" checkbox is checked and the numeric input shows 64

#### Scenario: Editing a schedule without a brightness
- **WHEN** the user activates Edit on a schedule with no brightness value set
- **THEN** the "Set brightness" checkbox is unchecked and the numeric input is disabled
