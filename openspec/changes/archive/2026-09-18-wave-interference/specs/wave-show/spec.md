# wave-show Specification

## Purpose

The Wave show produces a moving wave pattern on the LED strip. After this refresh, the wave geometry is driven by a source that oscillates along the strip, producing reflection at the ends and self-interference where overlapping wavefronts meet.

## ADDED Requirements

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

The brightness envelope at pixel `i` and iteration `t` SHALL be `exp(-decay_rate * |i - source_pos(t)| / N)`, where `decay_rate` is a constructor parameter.

#### Scenario: Pixels adjacent to the source are brightest

- **WHEN** source position is at pixel `M` at iteration `t`
- **THEN** pixel `M`'s envelope factor is `1.0` (highest possible) and pixels `M ± k` have strictly smaller envelope factors for `k > 0`

#### Scenario: Symmetric lighting when source is mid-strip

- **WHEN** source position is near the middle of the strip (e.g., `N/2`)
- **THEN** pixels at equal distance on either side of the source have equal envelope factors

### Requirement: Wave amplitude uses signed sine with absolute brightness

The wave amplitude at pixel `i` and iteration `t` SHALL be `sin(2π * (i - source_pos(t)) / wavelength)`, where `wavelength` is a constructor parameter in pixels. The brightness contribution from the wave SHALL be the absolute value of this sine (so the strip remains positive-valued and lit).

#### Scenario: Whole strip is non-negative brightness

- **WHEN** the show executes for any iteration
- **THEN** every pixel has non-negative brightness contribution from the wave term

#### Scenario: Wavelength controls wave density

- **WHEN** `wavelength` is small (e.g., 2 pixels), more wave crests fit on the strip
- **THEN** more local maxima and minima are visible per unit length
- **WHEN** `wavelength` is large (e.g., 20 pixels), fewer crests fit
- **THEN** fewer local maxima and minima are visible per unit length

### Requirement: Wave produces reflected waves at the strip ends

When the source is moving toward one end, the wavefronts it emits travel in the direction of motion. When the source reverses at an end, NEW wavefronts are emitted in the opposite direction while OLD wavefronts from previous source positions continue propagating. The result SHALL be that, at any moment after the first bounce, wavefronts traveling in both directions coexist on the strip.

#### Scenario: Coexisting wavefronts after first bounce

- **WHEN** the show has executed for at least one full source period
- **THEN** pixels both ahead of and behind the current source position show non-black wave amplitude (proving both directions of propagation are active)

### Requirement: Color follows wavefront emission time

Each pixel's color SHALL be derived from a `wheel()` hue index based on the time at which the wavefront that is currently at that pixel was emitted by the source. The hue SHALL drift continuously as wavefronts age, producing the existing rainbow-trailing look.

#### Scenario: Hue varies along the strip

- **WHEN** the show is executing at any iteration
- **THEN** pixels at different positions have different hue indices (proving each pixel's hue is tied to its own emission time, not stripe-uniformly)

### Requirement: Wave accepts three parameters

The Wave show SHALL accept exactly these parameters: `decay_rate` (float, default `2.0`), `brightness_frequency` (float, default `0.1`), `wavelength` (float, default `6.0`).

#### Scenario: Default parameters when params_json is empty

- **WHEN** Wave is constructed with no parameters or `params_json == "{}"`
- **THEN** `decay_rate` is `2.0`, `brightness_frequency` is `0.1`, `wavelength` is `6.0`

#### Scenario: All three parameters parsed from JSON

- **WHEN** Wave is created with `params_json == "{\"decay_rate\":3.5,\"brightness_frequency\":0.5,\"wavelength\":10.0}"`
- **THEN** the constructed show has `decay_rate=3.5`, `brightness_frequency=0.5`, `wavelength=10.0`

#### Scenario: Partial parameters use defaults

- **WHEN** Wave is created with `params_json == "{\"wavelength\":12.0}"`
- **THEN** `wavelength` is `12.0` and `decay_rate` defaults to `2.0`, `brightness_frequency` to `0.1`

### Requirement: wave_speed is no longer accepted

The Wave show SHALL NOT use a `wave_speed` parameter. JSON input containing a `wave_speed` field SHALL be silently ignored.

#### Scenario: wave_speed in JSON has no effect

- **WHEN** Wave is created with `params_json == "{\"wave_speed\":5.0,\"wavelength\":6.0}"`
- **THEN** the constructed show has `wavelength=6.0` and `wave_speed` is not stored or used

#### Scenario: Existing NVS configs with wave_speed still load

- **WHEN** an NVS-stored config from before this change containing `"wave_speed":N` is loaded
- **THEN** Wave is constructed with `decay_rate=2.0`, `brightness_frequency=0.1`, `wavelength=6.0` (defaults); no error is raised

### Requirement: Wave persistence

The Wave show's parameters SHALL persist across reboots via the existing `params_json` NVS storage, following the same mechanism as other configurable shows.

#### Scenario: Parameters restored after reboot

- **WHEN** Wave is active with `decay_rate=3.5, brightness_frequency=0.5, wavelength=10.0` and the device reboots
- **THEN** on next boot, Wave is reconstructed with `decay_rate=3.5, brightness_frequency=0.5, wavelength=10.0`

### Requirement: Computational profile unchanged

The Wave show's `execute()` SHALL perform at most one `sin`, one `exp`, one `fabs`, and one `wheel` call per pixel per iteration, and SHALL NOT allocate memory inside `execute()`.

#### Scenario: No per-pixel allocation

- **WHEN** `execute()` is reviewed or instrumented for heap allocations
- **THEN** zero heap allocations occur per pixel per iteration
- **THEN** `strip.setPixelColor` is called exactly `N` times per iteration

### Requirement: No new web UI parameters

The web UI SHALL NOT introduce any new input controls for Wave as part of this change. Existing Wave params UI (if any) SHALL continue to operate against the new parameter set without modification.

#### Scenario: Web UI still shows existing Wave parameters

- **WHEN** the user opens the show parameters section for Wave in the web UI
- **THEN** the same controls (and only those controls) are shown that were shown before this change
- **THEN** adjusting those controls and clicking Apply still produces a valid `/api/show` POST with the three accepted parameters