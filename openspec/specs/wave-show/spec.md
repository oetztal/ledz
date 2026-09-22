# wave-show Specification

## Purpose
TBD - created by archiving change wave-interference. Update Purpose after archive.
## Requirements
### Requirement: Wave source oscillates along the strip

The Wave show's source position SHALL oscillate smoothly between the two ends of the strip following `source_pos(t) = (N - 1) * 0.5 * (1 - cos(t * 2π * brightness_frequency))`, where `N` is the strip length and `brightness_frequency` is a constructor parameter in cycles per second.

#### Scenario: Source at left end at t=0

- **WHEN** the show is executed at `iteration = 0` (or any multiple of `1 / brightness_frequency` iterations)
- **THEN** the wave source position is at pixel 0 (or within rounding of it)

#### Scenario: Source at right end at half-period

- **WHEN** the show is executed at `iteration = 1 / (2 * brightness_frequency)` iterations
- **THEN** the wave source position is at pixel `N - 1` (or within rounding of it)

#### Scenario: Source motion has continuous velocity

- **WHEN** source position is sampled at successive iterations across a bounce
- **THEN** successive positions differ by a non-zero amount on either side of the bounce instant (no instantaneous jump)

### Requirement: Brightness decays symmetrically from the source

The brightness envelope at pixel `i` and iteration `t` SHALL be `exp(-decay_rate * |i - source_pos(t)| / N)`, where `decay_rate` is a constructor parameter and `N` is the strip length. Decay is measured per unit of strip length, so a 60-pixel strip and a 144-pixel strip will show different absolute falloffs for the same `decay_rate` (a wider strip has a more gradual falloff in pixels but the same fractional falloff across the strip). The envelope applies in both `bounce` and `traveling` modes.

#### Scenario: Pixels adjacent to the source are brightest

- **WHEN** source position is at pixel `M` at iteration `t` in any mode
- **THEN** pixel `M`'s envelope factor is `1.0` (highest possible) and pixels `M ± k` have strictly smaller envelope factors for `k > 0`

#### Scenario: Symmetric lighting when source is mid-strip

- **WHEN** source position is near the middle of the strip (e.g., `N/2`) in any mode
- **THEN** pixels at equal distance on either side of the source have equal envelope factors

### Requirement: Wave produces reflected waves at the strip ends

When `mode == "bounce"` and the source is moving toward one end, the wavefronts it emits travel in the direction of motion. When the source reverses at an end, NEW wavefronts are emitted in the opposite direction while OLD wavefronts from previous source positions continue propagating. The result SHALL be that, at any moment after the first bounce, wavefronts traveling in both directions coexist on the strip.

#### Scenario: Coexisting wavefronts after first bounce

- **WHEN** mode is `"bounce"` and the show has executed for at least one full source period
- **THEN** pixels both ahead of and behind the current source position show non-black wave amplitude (proving both directions of propagation are active)

### Requirement: Color follows wavefront emission time

Each pixel's color SHALL be derived from a `wheel()` hue index based on the time at which the wavefront that is currently at that pixel was emitted by the source. The hue SHALL drift continuously as wavefronts age, producing the existing rainbow-trailing look.

#### Scenario: Hue varies along the strip

- **WHEN** the show is executing at any iteration
- **THEN** pixels at different positions have different hue indices (proving each pixel's hue is tied to its own emission time, not stripe-uniformly)

### Requirement: Wave accepts parameters

The Wave show SHALL accept exactly these parameters: `mode` (string, default `"bounce"`), `decay_rate` (float, default `2.0`), `brightness_frequency` (float, default `0.1`). The `wavelength` parameter is no longer accepted; JSON input containing a `wavelength` field SHALL be silently ignored. The `mode` parameter is currently accepted but unused — both `"bounce"` and `"traveling"` produce identical output today, by design.

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

### Requirement: Wave is the reference for its own preview

The Wave show's preview in `scripts/build_pages.py` and the GitHub Pages gallery SHALL be produced by driving the Wave C++ source against a `MockStrip` via the show simulator binary (`[env:native_show_sim]`). There SHALL NOT be a separate hand-written Python re-implementation of the Wave algorithm in the repository. Any change to `src/show/Wave.cpp` is reflected in the next gallery regeneration with no further code changes.

#### Scenario: Gallery Wave preview matches the device

- **WHEN** the gallery is regenerated
- **THEN** `docs/show_previews/Wave.png` is produced by the simulator binary, not by a Python port
- **THEN** every pixel in `Wave.png` equals the value the device would write to the corresponding LED at the corresponding iteration

#### Scenario: No parallel Python port exists

- **WHEN** the repository is searched for a Python implementation of the Wave algorithm
- **THEN** no match is found outside `scripts/build_pages.py`'s subprocess invocation of the simulator binary

### Requirement: wave_speed is no longer accepted

The Wave show SHALL NOT use a `wave_speed` parameter. JSON input containing a `wave_speed` field SHALL be silently ignored.

#### Scenario: wave_speed in JSON has no effect

- **WHEN** Wave is created with `params_json == "{\"wave_speed\":5.0,\"wavelength\":6.0}"`
- **THEN** the constructed show has `wavelength=6.0` and `wave_speed` is not stored or used

#### Scenario: Existing NVS configs with wave_speed still load

- **WHEN** an NVS-stored config from before this change containing `"wave_speed":N` is loaded
- **THEN** Wave is constructed with `decay_rate=2.0`, `brightness_frequency=0.1`, `wavelength=6.0` (defaults); no error is raised

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

The Wave parameter section of the control page SHALL include a `Mode` selector with options `Bounce` and `Traveling`, a `Decay Rate` input, and a `Brightness Frequency` input. Selecting a mode and clicking Apply SHALL POST `{name: "Wave", params: {mode, decay_rate, brightness_frequency}}` to `/api/show`. The page SHALL NOT include a `Wavelength` (or `wavelength`) input. The page SHALL NOT include a `Wave Speed` (or `wave_speed`) input. The parameter section SHALL also include a `Preset` dropdown listing the variants `default`, `tight`, and `calm` from `scripts/show_variants.json`. Selecting a preset SHALL set the `Decay Rate` and `Brightness Frequency` inputs to the variant's `decay_rate` and `brightness_frequency` values and SHALL set the `Mode` selector to `Bounce`. Selecting a preset SHALL NOT send a request to the device.

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

#### Scenario: Selecting a preset fills decay and frequency and resets Mode

- **WHEN** the user picks `Tight fast` from the Preset dropdown
- **THEN** the `Decay Rate` input shows `4.0` and the `Brightness Frequency` input shows `0.4`
- **THEN** the `Mode` selector reads `Bounce`
- **THEN** no request is sent to the device

#### Scenario: Preset dropdown resets after the server echoes applied params

- **WHEN** the user picks `Tight fast`, clicks Apply Parameters, and the next status poll populates the inputs from `show_params`
- **THEN** the Preset dropdown reads `-- select a preset --` while the numeric inputs continue to show the applied values