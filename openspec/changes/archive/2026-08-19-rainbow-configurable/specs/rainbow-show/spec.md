## ADDED Requirements

### Requirement: Rainbow hue progression
The Rainbow show SHALL advance hue across the strip such that pixel `i` at iteration `t` is rendered with hue `((t * time_step) + (i * pixel_step)) mod 255`.

#### Scenario: Default parameters reproduce prior behavior
- **WHEN** Rainbow is created without parameters or with `params_json == "{}"`
- **THEN** `time_step` defaults to `1.0` and `pixel_step` defaults to `1.0`
- **THEN** the rendered animation is visually equivalent to the prior fixed `wheel((iteration + index) % 255)` behavior

#### Scenario: Parameters parsed from JSON
- **WHEN** Rainbow is created with `params_json == "{\"time_step\":2.5,\"pixel_step\":0.5}"`
- **THEN** `time_step` is `2.5` and `pixel_step` is `0.5`

#### Scenario: Partial parameters use defaults
- **WHEN** Rainbow is created with `params_json == "{\"time_step\":3.0}"`
- **THEN** `time_step` is `3.0` and `pixel_step` defaults to `1.0`

### Requirement: Rainbow parameter ranges
The Rainbow show SHALL accept any finite float value for `time_step` and `pixel_step`. The web UI SHALL restrict sliders to the range `0.0` to `5.0` with step `0.05`, but the C++ implementation SHALL NOT reject out-of-range or negative values received via the API.

#### Scenario: Slider clamps to non-negative in the UI
- **WHEN** the user opens the Rainbow params section in the web UI
- **THEN** both sliders display the value `1.0` by default
- **THEN** the `min` attribute of both sliders is `0`

#### Scenario: API accepts out-of-range values
- **WHEN** a client POSTs to `/api/show` with `params == {"time_step":-1.0,"pixel_step":20.0}`
- **THEN** the factory constructs a Rainbow show with those values; the API responds with success

### Requirement: Rainbow persistence
The Rainbow show's parameters SHALL persist across reboots via the existing `params_json` NVS storage, following the same mechanism as other configurable shows.

#### Scenario: Parameters restored after reboot
- **WHEN** Rainbow is active with `time_step=2.5, pixel_step=0.5` and the device reboots
- **THEN** on next boot, Rainbow is reconstructed with `time_step=2.5, pixel_step=0.5`

### Requirement: Rainbow web UI controls
The web control interface SHALL expose a Rainbow params section with two range sliders (one per parameter), a live numeric value display, and an Apply button. The section SHALL be visible only when Rainbow is the selected show.

#### Scenario: Section appears for Rainbow selection
- **WHEN** the user selects "Rainbow" from the show dropdown
- **THEN** the Rainbow params section becomes visible
- **THEN** other params sections (Fire, Wave, etc.) are hidden

#### Scenario: Apply button POSTs parameters
- **WHEN** the user adjusts sliders and clicks "Apply Parameters"
- **THEN** the page POSTs to `/api/show` with `{"name":"Rainbow","params":{"time_step":<value>,"pixel_step":<value>}}`

#### Scenario: Sliders populate from stored params on load
- **WHEN** the page loads with Rainbow active and stored params `{"time_step":2.5,"pixel_step":0.5}`
- **THEN** the time_step slider displays `2.5` and the pixel_step slider displays `0.5`