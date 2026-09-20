## REMOVED Requirements

### Requirement: Wave amplitude uses signed sine with absolute brightness
**Reason**: The `|sin(phase)|` term existed to produce fine wavelength-controlled stripes layered on top of the bouncing source. The wavelength parameter is being removed (see MODIFIED "Wave accepts parameters"), so the phase term it fed has no source. The reference implementation (`scripts/wave_show.py`) has no phase term at all — the per-pixel brightness is just `source_brightness * envelope`.
**Migration**: None. The show's brightness now comes entirely from the bouncing source's own brightness oscillation and the distance-decay envelope. The resulting look is a bouncing rainbow source whose brightness fades smoothly with distance from it.

### Requirement: Bounce mode phase is source-relative
**Reason**: Phase was `2π * (i - source_pos(t)) / wavelength` and depended on the now-removed `wavelength` parameter. Without a phase term there is no "source-relative" phase to be source-relative *to*.
**Migration**: None. The mode is still accepted by the JSON contract for future expansion but currently produces identical output to `traveling` (and to no mode).

### Requirement: Traveling mode phase drifts at the documented rate
**Reason**: Phase was `2π * (i / wavelength - t * brightness_frequency)` and depended on the now-removed `wavelength` parameter. Without a phase term there are no stripes for the drift rate to describe.
**Migration**: None. The mode is still accepted by the JSON contract for future expansion but currently produces identical output to `bounce`.

## MODIFIED Requirements

### Requirement: Wave accepts parameters

The Wave show SHALL accept exactly these parameters: `mode` (string, default `"bounce"`), `decay_rate` (float, default `2.0`), `brightness_frequency` (float, default `0.1`). The `wavelength` parameter is no longer accepted; JSON input containing a `wavelength` field SHALL be silently ignored. The `mode` parameter is currently accepted but unused — both `"bounce"` and `"traveling"` produce identical output today, matching the reference implementation.

#### Scenario: Default parameters when params_json is empty

- **WHEN** Wave is constructed with no parameters or `params_json == "{}"`
- **THEN** `mode` is `"bounce"`, `decay_rate` is `2.0`, `brightness_frequency` is `0.1`

#### Scenario: All parameters parsed from JSON

- **WHEN** Wave is created with `params_json == "{\"mode\":\"bounce\",\"decay_rate\":3.5,\"brightness_frequency\":0.5}"`
- **THEN** the constructed show has `mode="bounce"`, `decay_rate=3.5`, `brightness_frequency=0.5`

#### Scenario: Partial parameters use defaults

- **WHEN** Wave is created with `params_json == "{\"brightness_frequency\":0.2}"`
- **THEN** `brightness_frequency` is `0.2` and `mode` defaults to `"bounce"`, `decay_rate` to `2.0`

#### Scenario: Unknown mode values fall back to bounce

- **WHEN** Wave is created with `params_json == "{\"mode\":\"bogus\"}"`
- **THEN** the constructed show has `mode="bounce"` (no error is raised)

#### Scenario: wavelength is silently ignored

- **WHEN** Wave is created with `params_json == "{\"wavelength\":12.0}"`
- **THEN** the show constructs successfully and `wavelength` has no effect on the rendered pixels

#### Scenario: bounce and traveling modes produce identical pixels

- **WHEN** two Wave shows are created with the same `decay_rate` and `brightness_frequency` but different `mode` values, and both are executed against strips of the same length for the same number of iterations
- **THEN** every pixel is identical between the two strips

### Requirement: Wave persistence

The Wave show's parameters SHALL persist across reboots via the existing `params_json` NVS storage, following the same mechanism as other configurable shows. Only `mode`, `decay_rate`, and `brightness_frequency` are stored and restored; any other field (including the legacy `wavelength` and the older `wave_speed`) is silently dropped.

#### Scenario: Parameters restored after reboot

- **WHEN** Wave is active with `decay_rate=3.5, brightness_frequency=0.5` and the device reboots
- **THEN** on next boot, Wave is reconstructed with `decay_rate=3.5, brightness_frequency=0.5`

### Requirement: Computational profile unchanged

The Wave show's `execute()` SHALL perform at most one `exp`, one `fabs`, and one `wheel` call per pixel per iteration, and SHALL NOT allocate memory inside `execute()`.

#### Scenario: No per-pixel allocation

- **WHEN** `execute()` is reviewed or instrumented for heap allocations
- **THEN** zero heap allocations occur per pixel per iteration
- **THEN** `strip.setPixelColor` is called exactly `N` times per iteration

### Requirement: Web UI exposes the Wave parameters

The Wave parameter section of the control page SHALL include a `Mode` selector with options `Bounce` and `Traveling`, a `Decay Rate` input, and a `Brightness Frequency` input. Selecting a mode and clicking Apply SHALL POST `{name: "Wave", params: {mode, decay_rate, brightness_frequency}}` to `/api/show`. The page SHALL NOT include a `Wavelength` (or `wavelength`) input. The page SHALL NOT include a `Wave Speed` (or `wave_speed`) input.

#### Scenario: Parameter section is shown when Wave is selected

- **WHEN** the user selects `Wave` from the show dropdown
- **THEN** the parameter section contains a `Mode` selector with `Bounce` and `Traveling` options visible
- **THEN** the parameter section contains a `Decay Rate` input and a `Brightness Frequency` input
- **THEN** the parameter section does not contain a `Wavelength` (or `wavelength`) input
- **THEN** the parameter section does not contain a `Wave Speed` (or `wave_speed`) input

#### Scenario: Apply Parameters sends the three-param payload

- **WHEN** the user selects `Traveling`, adjusts decay/frequency, and clicks Apply Parameters
- **THEN** a `POST /api/show` request is sent with body `{"name":"Wave","params":{"mode":"traveling", "decay_rate":..., "brightness_frequency":...}}`
- **THEN** the request body does not contain a `wavelength` field
