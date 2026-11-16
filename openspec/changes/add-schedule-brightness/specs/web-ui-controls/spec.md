## MODIFIED Requirements

### Requirement: Edit mode loads a schedule into the form and updates it in place
Activating Edit SHALL load the schedule's time, action, preset, weekdays and, for a "Load preset" schedule, its brightness into the schedule form, switch the form's submit label to indicate an update, show a cancel-edit control, and SHALL NOT send a request. Submitting the form in edit mode SHALL update that schedule in place and return the form to create mode.

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

#### Scenario: Editing a schedule with a brightness
- **WHEN** the user activates Edit on a "Load preset" schedule whose brightness is 64
- **THEN** the "Set brightness" checkbox is checked and the numeric input shows 64

#### Scenario: Editing a schedule without a brightness
- **WHEN** the user activates Edit on a schedule with no brightness value set
- **THEN** the "Set brightness" checkbox is unchecked and the numeric input is disabled

## ADDED Requirements

### Requirement: Schedule cards show their brightness
A schedule card SHALL show the schedule's brightness value when one is set, and SHALL show no brightness indicator when no brightness value is set.

#### Scenario: Card shows the brightness
- **WHEN** a schedule with brightness 64 is listed
- **THEN** its card indicates brightness 64

#### Scenario: Card without a brightness
- **WHEN** a schedule with no brightness value set is listed
- **THEN** its card shows no brightness indicator

### Requirement: The schedule form offers a staged brightness control
The schedule form SHALL provide a control for an optional brightness value consisting of a checkbox that stages whether a brightness is set and a numeric control that accepts a value from 0 to 255. The control SHALL be shown only while the action is "Load preset", mirroring the preset selector, because "Turn off LEDs" schedules ignore brightness. The control SHALL NOT rely on inline `style` attributes for its layout.

#### Scenario: Preset loads show the control
- **WHEN** the action is "Load preset"
- **THEN** the "Set brightness" checkbox and numeric input are shown

#### Scenario: Turn-off schedules have no brightness control
- **WHEN** the action is "Turn off LEDs"
- **THEN** the "Set brightness" control is not shown
- **THEN** the request omits `brightness`

### Requirement: The brightness checkbox stages its value instead of acting immediately
The brightness control SHALL use a checkbox, not a toggle switch, because it stages state that is applied only when the form is submitted.

#### Scenario: Checkbox stages the value
- **WHEN** the action is "Load preset" and the user checks "Set brightness" and enters 64
- **THEN** no request is sent until the form is submitted
- **THEN** the numeric input is enabled

### Requirement: The staged brightness control defaults to unset
When the control is shown it SHALL default to unchecked with the numeric input disabled.

#### Scenario: New schedules default to unset
- **WHEN** the schedule form is displayed in create mode with the action "Load preset"
- **THEN** the "Set brightness" checkbox is unchecked and the numeric input is disabled

### Requirement: The staged brightness control sends only a checked value
The numeric control SHALL be enabled while the checkbox is checked and disabled while it is unchecked. The form SHALL include `brightness` in its request only when the checkbox is checked, using the numeric control's value.

#### Scenario: Checked brightness is sent
- **WHEN** the user submits the form with "Set brightness" checked at 64
- **THEN** the request includes `brightness` 64
- **THEN** the numeric input is enabled

#### Scenario: Unchecked omits the brightness
- **WHEN** the user submits the form with "Set brightness" unchecked
- **THEN** the request omits `brightness`
