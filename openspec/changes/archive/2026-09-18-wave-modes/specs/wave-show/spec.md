## MODIFIED Requirements

### Requirement: Brightness decays symmetrically from the source

The brightness envelope at pixel `i` and iteration `t` SHALL be `exp(-decay_rate * |i - source_pos(t)| / N)`, where `decay_rate` is a constructor parameter and `N` is the strip length. Decay is measured per unit of strip length, so a 60-pixel strip and a 144-pixel strip will show different absolute falloffs for the same `decay_rate` (a wider strip has a more gradual falloff in pixels but the same fractional falloff across the strip). The envelope applies in both `bounce` and `traveling` modes.

#### Scenario: Pixels adjacent to the source are brightest

- **WHEN** source position is at pixel `M` at iteration `t` in any mode
- **THEN** pixel `M`'s envelope factor is `1.0` (highest possible) and pixels `M ± k` have strictly smaller envelope factors for `k > 0`

#### Scenario: Symmetric lighting when source is mid-strip

- **WHEN** source position is near the middle of the strip (e.g., `N/2`) in any mode
- **THEN** pixels at equal distance on either side of the source have equal envelope factors

### Requirement: Wave amplitude uses signed sine with absolute brightness

The wave amplitude at pixel `i` and iteration `t` SHALL be `|sin(phase(i, t))|`, where `phase(i, t)` is mode-dependent (see the per-mode phase requirements). The brightness contribution from the wave SHALL be non-negative so the strip stays lit.

#### Scenario: Whole strip is non-negative brightness

- **WHEN** the show executes for any iteration in any mode
- **THEN** every pixel has non-negative brightness contribution from the wave term

#### Scenario: Wavelength controls wave density

- **WHEN** `wavelength` is small (e.g., 2 pixels) in any mode
- **THEN** more local maxima and minima are visible per unit length
- **WHEN** `wavelength` is large (e.g., 20 pixels) in any mode
- **THEN** fewer local maxima and minima are visible per unit length

### Requirement: Wave produces reflected waves at the strip ends

When `mode == "bounce"` and the source is moving toward one end, the wavefronts it emits travel in the direction of motion. When the source reverses at an end, NEW wavefronts are emitted in the opposite direction while OLD wavefronts from previous source positions continue propagating. The result SHALL be that, at any moment after the first bounce, wavefronts traveling in both directions coexist on the strip.

#### Scenario: Coexisting wavefronts after first bounce

- **WHEN** mode is `"bounce"` and the show has executed for at least one full source period
- **THEN** pixels both ahead of and behind the current source position show non-black wave amplitude (proving both directions of propagation are active)

### Requirement: Wave accepts three parameters

The Wave show SHALL accept exactly these parameters: `mode` (string, default `"bounce"`), `decay_rate` (float, default `2.0`), `brightness_frequency` (float, default `0.1`), `wavelength` (float, default `6.0`).

#### Scenario: Default parameters when params_json is empty

- **WHEN** Wave is constructed with no parameters or `params_json == "{}"`
- **THEN** `mode` is `"bounce"`, `decay_rate` is `2.0`, `brightness_frequency` is `0.1`, `wavelength` is `6.0`

#### Scenario: All four parameters parsed from JSON

- **WHEN** Wave is created with `params_json == "{\"mode\":\"traveling\",\"decay_rate\":3.5,\"brightness_frequency\":0.5,\"wavelength\":10.0}"`
- **THEN** the constructed show has `mode="traveling"`, `decay_rate=3.5`, `brightness_frequency=0.5`, `wavelength=10.0`

#### Scenario: Partial parameters use defaults

- **WHEN** Wave is created with `params_json == "{\"wavelength\":12.0}"`
- **THEN** `wavelength` is `12.0` and `mode` defaults to `"bounce"`, `decay_rate` to `2.0`, `brightness_frequency` to `0.1`

#### Scenario: Unknown mode values fall back to bounce

- **WHEN** Wave is created with `params_json == "{\"mode\":\"bogus\"}"`
- **THEN** the constructed show has `mode="bounce"` (no error is raised)

## REMOVED Requirements

### Requirement: No new web UI parameters
**Reason**: This change adds a `mode` dropdown to the Wave parameter section of the web UI. The previous "no new web UI parameters" gate was written before the mode concept existed and is now superseded by the requirement that the UI SHALL expose the `mode` parameter.
**Migration**: None. NVS-stored configs are unaffected. Users who do not interact with the new dropdown see no change.

## ADDED Requirements

### Requirement: Bounce mode phase is source-relative

When `mode == "bounce"`, the wave phase at pixel `i` and iteration `t` SHALL be `2π * (i - source_pos(t)) / wavelength`. Wavefronts are therefore phase-locked to the source and move with it.

#### Scenario: Bounce mode stripes follow source motion

- **WHEN** mode is `"bounce"` and source position moves from pixel `M₁` at iteration `t₁` to pixel `M₂` at iteration `t₂` (where `M₂ - M₁ > 0`)
- **THEN** the bright stripes of the wave amplitude have shifted by approximately `M₂ - M₁` pixels over the same interval (in the same direction as the source)

### Requirement: Traveling mode phase drifts at the documented rate

When `mode == "traveling"`, the wave phase at pixel `i` and iteration `t` SHALL be `2π * (i / wavelength - t * brightness_frequency)`. Stripes SHALL drift across the strip at `brightness_frequency × wavelength` pixels per second, independent of source motion.

#### Scenario: Traveling mode stripes drift at the documented rate

- **WHEN** mode is `"traveling"` with `brightness_frequency = 0.5` and `wavelength = 4.0`
- **THEN** between any two iterations separated by one second, the bright stripes of the wave amplitude have shifted by `0.5 × 4.0 = 2.0` pixels

#### Scenario: Traveling mode drift direction follows sign of frequency

- **WHEN** mode is `"traveling"` with `brightness_frequency < 0`
- **THEN** stripes drift in the opposite direction along the strip compared to the same configuration with `brightness_frequency > 0`

#### Scenario: Traveling mode keeps the bouncing envelope

- **WHEN** mode is `"traveling"` and source position is at pixel `M`
- **THEN** pixels adjacent to `M` are brightest and pixels at `M ± k` are dimmer (the bouncing envelope still applies)

### Requirement: Web UI exposes the Wave mode parameter

The Wave parameter section of the control page SHALL include a `Mode` selector with options `Bounce` and `Traveling`. Selecting a mode and clicking Apply SHALL POST `{name: "Wave", params: {mode, decay_rate, brightness_frequency, wavelength}}` to `/api/show`. The page SHALL NOT include a `Wave Speed` (or `wave_speed`) input.

#### Scenario: Mode dropdown is shown when Wave is selected

- **WHEN** the user selects `Wave` from the show dropdown
- **THEN** the parameter section contains a `Mode` selector with `Bounce` and `Traveling` options visible
- **THEN** the parameter section does not contain a `Wave Speed` (or `wave_speed`) input

#### Scenario: Apply Parameters sends the four-param payload

- **WHEN** the user selects `Traveling`, adjusts decay/freq/wavelength, and clicks Apply Parameters
- **THEN** a `POST /api/show` request is sent with body `{"name":"Wave","params":{"mode":"traveling", "decay_rate":..., "brightness_frequency":..., "wavelength":...}}`