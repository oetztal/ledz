## MODIFIED Requirements

### Requirement: Brightness decays symmetrically from the source

The brightness envelope at pixel `i` and iteration `t` SHALL be `exp(-decay_rate * |i - source_pos(t)| / N)`, where `decay_rate` is a constructor parameter and `N` is the strip length. Decay is measured per unit of strip length, so a 60-pixel strip and a 144-pixel strip will show different absolute falloffs for the same `decay_rate` (a wider strip has a more gradual falloff in pixels but the same fractional falloff across the strip).

#### Scenario: Pixels adjacent to the source are brightest

- **WHEN** source position is at pixel `M` at iteration `t`
- **THEN** pixel `M`'s envelope factor is `1.0` (highest possible) and pixels `M ± k` have strictly smaller envelope factors for `k > 0`

#### Scenario: Symmetric lighting when source is mid-strip

- **WHEN** source position is near the middle of the strip (e.g., `N/2`)
- **THEN** pixels at equal distance on either side of the source have equal envelope factors

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

## REMOVED Requirements

### Requirement: Source brightness fades near the strip ends
**Reason**: The end-fade factor was added in `wave-polish` to mitigate the cosine source's hot spot at the extremes, but in practice it dimmed the source 60 % of the time and made the show look substantially dimmer than the version it replaced. The reverted implementation removes the factor entirely; the cosine source's brief stop at each end is mild and does not need correction.
**Migration**: None. No NVS-stored values are affected. The behaviour change is limited to the source-brightness multiplier and is observable only in the on-strip visual output.

#### Scenario: Source at strip end contributes no amplitude
- **WHEN** source position is at pixel `0` or pixel `N - 1`
- **THEN** the end-fade factor is `0.0` and the source contributes no brightness to any pixel (regardless of the wave term or envelope)

#### Scenario: Source mid-strip is unaffected by end-fade
- **WHEN** source position is at pixel `floor((N - 1) / 2)` (i.e., the middle of the strip)
- **THEN** the end-fade factor is `1.0` and the source brightness is unmodified

#### Scenario: End-fade ramps linearly with source position
- **WHEN** source position is at one quarter of the way from one end (e.g., `(N - 1) * 0.25` on a 60-pixel strip, pixel `14`)
- **THEN** the end-fade factor is `0.5`