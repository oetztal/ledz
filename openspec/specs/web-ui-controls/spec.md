# web-ui-controls Specification

## Purpose
Defines how boolean and other form controls in the web UI are rendered, styled and made accessible: which visual component a boolean control gets based on when its change takes effect, how container-scoped input styling must be scoped to avoid breaking checkboxes and radios, and the keyboard, labelling, touch-target and reduced-motion guarantees every boolean control provides.

## Requirements

### Requirement: Form input styling scoped by container excludes checkboxes and radios
A CSS rule that styles inputs by their container SHALL exclude `input[type="checkbox"]` and `input[type="radio"]`. Such rules SHALL NOT be removed outright when input types in the UI depend on them for their only styling.

#### Scenario: Checkbox in a form group
- **WHEN** an `input[type="checkbox"]` is placed inside a `.form-group`
- **THEN** it does not receive the full-width, padded, bordered text-input treatment
- **THEN** it renders at its component size with its label text on the same line

#### Scenario: Time input keeps its styling
- **WHEN** the schedule time field (`input[type="time"]` inside a `.form-group`) is displayed
- **THEN** it retains the same padding, border, border-radius and font size as the other text-like inputs on the page

#### Scenario: Radio input added later
- **WHEN** an `input[type="radio"]` is placed inside a `.form-group`
- **THEN** it is likewise excluded from the container-scoped input styling

### Requirement: Boolean controls are rendered according to when they take effect
A boolean control whose change acts on the device immediately SHALL be rendered as a toggle switch. A boolean control that only stages state applied later by an explicit button SHALL be rendered as a checkbox.

#### Scenario: Touch control enable is a toggle
- **WHEN** the settings page displays the Touch Control section
- **THEN** "Enable touch control" is rendered as a toggle switch
- **THEN** changing it sends its request immediately, and a failed request restores the previous position

#### Scenario: Staged options are checkboxes
- **WHEN** the settings page displays the WiFi and Firmware Update sections, or the control page displays the colour-ranges parameters
- **THEN** "Open network (no password)", "Allow installing older or equal version" and "Gradient mode" are each rendered as a checkbox
- **THEN** none of them sends a request until its section's Update, Install or Apply button is pressed

#### Scenario: Open network is not a switch
- **WHEN** the user checks "Open network (no password)"
- **THEN** the control is a checkbox, not a toggle switch
- **THEN** the password input is disabled and cleared, and no request is sent to the device

### Requirement: The same boolean control renders identically on every page
A checkbox SHALL render the same way regardless of which container it is placed in, and SHALL NOT rely on inline `style` attributes for its layout or spacing.

#### Scenario: Checkbox in a control-page parameter row
- **WHEN** the "Gradient mode" checkbox is displayed inside a `.param-row` on the control page
- **THEN** it has the same box size, spacing and label alignment as the checkboxes inside a `.form-group` on the settings page
- **THEN** its markup carries no inline `style` attribute

### Requirement: Boolean controls are keyboard accessible with a visible focus indicator
Every boolean control SHALL be reachable by keyboard and SHALL show a visible focus indicator when focused via keyboard. A control that visually replaces its native input SHALL render the focus indicator on the element that stands in for it.

#### Scenario: Toggle switch focused by keyboard
- **WHEN** the user tabs to the "Enable touch control" toggle
- **THEN** a visible focus ring is drawn around the switch
- **THEN** pressing Space changes its state

#### Scenario: Checkbox focused by keyboard
- **WHEN** the user tabs to a checkbox
- **THEN** a visible focus indicator is shown
- **THEN** pressing Space toggles it

#### Scenario: Pointer interaction does not show the ring
- **WHEN** the user clicks a toggle switch with a pointer
- **THEN** no keyboard focus ring is drawn

### Requirement: Boolean control labels are associated and clickable
Every boolean control SHALL have its text label associated with the input, either by wrapping it or by a `for` attribute, and activating the label SHALL toggle the control. The label SHALL indicate interactivity with a pointer cursor.

#### Scenario: Clicking a checkbox label
- **WHEN** the user clicks the text "Open network (no password)"
- **THEN** the checkbox toggles

#### Scenario: Clicking a toggle label
- **WHEN** the user clicks the text "Enable touch control"
- **THEN** the toggle switches

### Requirement: Boolean controls meet a minimum touch target size
A boolean control row SHALL present an interactive area at least 44 pixels high.

#### Scenario: Checkbox row on a phone
- **WHEN** a checkbox row is displayed at a phone viewport width
- **THEN** the row's interactive area is at least 44 pixels high
- **THEN** a label wrapping to two lines keeps the box aligned to the first line rather than centred against the whole block

### Requirement: Toggle switches support a disabled state and reduced motion
The toggle switch component SHALL render a visually distinct disabled state, and SHALL suppress its transition animations when the user has requested reduced motion.

#### Scenario: Disabled toggle
- **WHEN** a toggle switch's input is disabled
- **THEN** the switch is visually dimmed and shows a not-allowed cursor

#### Scenario: Reduced motion requested
- **WHEN** the user's system requests reduced motion and a toggle is switched
- **THEN** the switch changes state without an animated transition

### Requirement: Boolean control state remains readable by existing scripts
Restyling a boolean control SHALL NOT change its element `id`, its `type`, or the event handler attached to it, so that page scripts reading and writing `.checked` continue to work unchanged.

#### Scenario: Script reads a restyled toggle
- **WHEN** `loadTouchConfig()` sets `document.getElementById('touchEnabled').checked` from the device response
- **THEN** the toggle switch displays the corresponding position

#### Scenario: Script reverts a failed change
- **WHEN** the touch enable request fails and the handler restores `.checked` to its previous value
- **THEN** the toggle switch returns to its previous position

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
