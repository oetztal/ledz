## ADDED Requirements

### Requirement: Control page exposes an Off action that switches the strip to all-black

The control page SHALL display a single "Off" button in the status bar alongside the existing "Current Show" and "Brightness" indicators. Activating the button SHALL change the active show to `Solid` with a single black colour (`colors=[[0,0,0]]`) by issuing a `POST /api/show` request with that payload. The button SHALL send exactly one request per activation.

#### Scenario: User activates Off
- **WHEN** the user clicks the Off button on the control page
- **THEN** the browser sends `POST /api/show` with body `{"name":"Solid","params":{"colors":[[0,0,0]]}}`
- **THEN** the LED strip renders all pixels at colour (0, 0, 0) within one render cycle
- **THEN** the device returns 200 to the request

#### Scenario: Off button is disabled while the request is in flight
- **WHEN** the user clicks the Off button and the request has not yet completed
- **THEN** the button is disabled and ignores further clicks until the request settles
- **WHEN** the request completes (success or failure)
- **THEN** the button is re-enabled

### Requirement: Off state survives a reboot

After the Off button has been activated, the device SHALL boot into the same all-black state on subsequent power cycles. The persisted state is the `Solid` show with `colors=[[0,0,0]]` written to NVS via the existing `SaveShowConfig` path; no additional persistence mechanism is required.

#### Scenario: Reboot after Off
- **WHEN** the Off button has been activated and the device is power-cycled
- **THEN** the active show on boot is `Solid` with `colors=[[0,0,0]]`
- **THEN** the LED strip renders all pixels at colour (0, 0, 0)

#### Scenario: Scheduled TURN_OFF and manual Off produce the same state
- **WHEN** the user clicks Off and later a schedule with `TimerAction::TURN_OFF` fires
- **THEN** both end with the device in the same persisted state
- **THEN** `/api/status` reports `current_show` as `Solid` and `show_params` as `{"colors":[[0,0,0]]}` in both cases

### Requirement: Control page reflects the post-Off state

After the Off request completes, the control page SHALL update the status bar, the show dropdown, the show description, and the parameter panel visibility so that all four reflect the new `Solid` show with all-black colours. The existing `updateStatus()` poll path SHALL be reused for this update.

#### Scenario: Status bar after Off
- **WHEN** the Off request completes successfully
- **THEN** "Current Show" in the status bar reads `Solid`
- **THEN** the show dropdown's selected option is `Solid`
- **THEN** the show description displays the description of the `Solid` show
- **THEN** the colour-ranges parameter panel becomes visible

### Requirement: Any other show selection reverses the Off state

The user SHALL be able to leave the Off state by selecting any show from the dropdown or by picking a non-black colour in the colour-ranges parameter panel and applying it. No dedicated "Turn back on" control SHALL be added.

#### Scenario: Selecting a different show
- **WHEN** the device is in the Off state and the user picks a show other than `Solid` from the dropdown
- **THEN** that show becomes active
- **THEN** the LED strip renders that show's output

#### Scenario: Picking a colour from the colour-ranges panel
- **WHEN** the device is in the Off state and the user changes a colour input or the gradient flag and clicks "Apply Pattern"
- **THEN** the `Solid` show runs with the new colour configuration
- **THEN** the LED strip renders the new colours