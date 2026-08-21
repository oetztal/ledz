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
- **WHEN** the Daily Alarm time field (`input[type="time"]` inside a `.form-group`) is displayed
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
