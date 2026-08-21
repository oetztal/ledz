# settings-page Specification

## Purpose
Defines the behavior of the device settings page and its supporting HTTP endpoints: how editable controls are prefilled from current device state, how modifications are tracked and submitted, and how WiFi credentials are handled.

## Requirements

### Requirement: Settings inputs prefilled from current device state
The settings page SHALL populate every editable settings control with the device's current value when the page loads. This applies to the device name, number of LEDs, LED pin, gamma mode, cycle time, touch enable, touch threshold, and WiFi SSID. Placeholder text SHALL remain as a fallback shown only when no current value is available.

#### Scenario: Hardware fields prefilled on load
- **WHEN** the settings page loads and `GET /api/status` returns `num_pixels: 240`, `led_pin: 39`, `cycle_time: 20`, `gamma_mode: 1`
- **THEN** the `numPixels` input has value `240`
- **THEN** the `ledPin` input has value `39`
- **THEN** the `cycleTime` select has value `20`
- **THEN** the `gammaMode` select has value `1`

#### Scenario: Device name prefilled on load
- **WHEN** the settings page loads and `GET /api/status` returns `device_name: "hallway"`
- **THEN** the `deviceName` input has value `hallway`

#### Scenario: WiFi SSID prefilled on load
- **WHEN** the settings page loads and `GET /api/status` returns `wifi_configured_ssid: "HomeNet"`
- **THEN** the `wifiSSID` input has value `HomeNet`
- **THEN** the `wifiPassword` input is empty

#### Scenario: Status request fails
- **WHEN** `GET /api/status` fails or returns a non-OK response
- **THEN** the inputs remain empty and show their placeholders
- **THEN** an error state is surfaced in the section's info box rather than silently leaving blank fields

### Requirement: WiFi password is never prefilled
The settings page SHALL NOT populate the WiFi password field, and no HTTP endpoint SHALL expose the stored WiFi password in any form, including masked, truncated, or length-hinted representations.

#### Scenario: Password absent from status response
- **WHEN** a client requests `GET /api/status`
- **THEN** the response contains no key holding the stored WiFi password or its length

### Requirement: Configured WiFi SSID exposed separately from connected SSID
`GET /api/status` SHALL include `wifi_configured_ssid`, read from the stored `WiFiConfig`, regardless of connection state. The existing `wifi_ssid` key SHALL continue to report the currently connected network and SHALL continue to be present only when the device is connected in station mode.

#### Scenario: Connected in station mode
- **WHEN** the device is connected to `HomeNet` and `HomeNet` is the stored SSID
- **THEN** `GET /api/status` returns `wifi_connected: true`, `wifi_ssid: "HomeNet"` and `wifi_configured_ssid: "HomeNet"`

#### Scenario: Running in AP mode
- **WHEN** the device is not connected in station mode but has stored SSID `HomeNet`
- **THEN** `GET /api/status` returns `wifi_connected: false`, omits `wifi_ssid`, and returns `wifi_configured_ssid: "HomeNet"`

#### Scenario: No credentials stored
- **WHEN** the device has never been configured
- **THEN** `GET /api/status` returns `wifi_configured_ssid` as an empty string
- **THEN** the `wifiSSID` input is empty and shows its placeholder

### Requirement: Only modified fields are submitted
The settings page SHALL retain a snapshot of the values loaded from the device and SHALL include a field in a submit request only when its current control value differs from the snapshot. Confirmation dialogs SHALL describe only the differing fields. When no field differs, the page SHALL report that nothing changed and SHALL NOT send a request.

#### Scenario: Single field changed
- **WHEN** the page loaded with `num_pixels: 300, led_pin: 35, cycle_time: 10, gamma_mode: 0` and the user changes only `ledPin` to `39` and submits
- **THEN** the confirmation dialog names only the LED pin change
- **THEN** the request body to `POST /api/settings/device` is `{"led_pin":39}`

#### Scenario: Nothing changed
- **WHEN** the user submits a section without altering any of its controls
- **THEN** no request is sent
- **THEN** the user is told that nothing changed

#### Scenario: Snapshot updated after a successful save
- **WHEN** a save succeeds and the device does not restart
- **THEN** the snapshot is updated to the newly saved values
- **THEN** the section is no longer reported as modified

### Requirement: Modified state is indicated with the saved value
Each section with prefilled inputs SHALL show, while any of its controls differ from the snapshot, an indication that the section is modified together with the saved value being edited away from. The section SHALL NOT display a redundant read-only echo of a value that is already shown in an input.

#### Scenario: Indicator appears on edit
- **WHEN** the page loaded with `num_pixels: 300` and the user types `240` into `numPixels`
- **THEN** the Hardware Configuration info box indicates the section is modified and names `300` as the saved number of LEDs

#### Scenario: Indicator clears on revert
- **WHEN** the user edits `numPixels` and then restores it to the loaded value
- **THEN** the modified indication is removed

#### Scenario: Static notices retained
- **WHEN** the settings page is displayed
- **THEN** the Hardware Configuration info box still states that the device will restart after updating

### Requirement: Cleared required fields are rejected
When a prefilled required field is submitted empty, the settings page SHALL reject the submission with a validation message and SHALL NOT treat the empty value as "leave unchanged". The WiFi password field is exempt.

#### Scenario: Cleared LED count
- **WHEN** the user clears the `numPixels` input and submits the Hardware Configuration section
- **THEN** a validation message is shown
- **THEN** no request is sent

#### Scenario: Cleared device name
- **WHEN** the user clears the `deviceName` input and submits
- **THEN** a validation message is shown
- **THEN** no request is sent

### Requirement: Blank WiFi password preserves the stored password
When the WiFi password field is left blank, the settings page SHALL omit the `password` key from the request body. `POST /api/settings/wifi` SHALL preserve the stored password when the request contains no `password` key, and SHALL overwrite it when the key is present — including with an empty string.

#### Scenario: SSID changed, password left blank
- **WHEN** the stored configuration is `ssid: "HomeNet", password: "secret"` and the user changes the SSID to `GuestNet` and submits with a blank password
- **THEN** the request body is `{"ssid":"GuestNet"}`
- **THEN** the stored configuration becomes `ssid: "GuestNet", password: "secret"`

#### Scenario: Password key absent on a direct API call
- **WHEN** a client POSTs `{"ssid":"HomeNet"}` to `/api/settings/wifi` and a password is stored
- **THEN** the stored password is unchanged

#### Scenario: Password key present and empty
- **WHEN** a client POSTs `{"ssid":"HomeNet","password":""}` to `/api/settings/wifi`
- **THEN** the stored password is cleared

#### Scenario: SSID still required
- **WHEN** a client POSTs a body with a missing or empty `ssid`
- **THEN** the endpoint responds `400` with an error, as before

### Requirement: Open network can be configured explicitly
The WiFi section SHALL provide an explicit control for configuring a network with no password. When it is active, the page SHALL send `password` as an empty string and SHALL disable the password input.

#### Scenario: Open network selected
- **WHEN** the user checks "Open network (no password)" and submits with SSID `CafeWiFi`
- **THEN** the password input is disabled
- **THEN** the request body is `{"ssid":"CafeWiFi","password":""}`
- **THEN** the stored password is cleared

### Requirement: AP-mode setup form is unaffected
The first-run WiFi setup page served in AP mode SHALL continue to present empty SSID and password fields and SHALL continue to use `POST /api/wifi`, whose credential-handling semantics are unchanged.

#### Scenario: First-run setup
- **WHEN** the device is in AP mode and the user opens the setup page
- **THEN** the SSID and password fields are empty
- **THEN** submitting posts to `/api/wifi` with both `ssid` and `password`
