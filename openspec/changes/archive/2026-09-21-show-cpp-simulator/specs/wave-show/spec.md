## MODIFIED Requirements

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

## ADDED Requirements

### Requirement: Wave is the reference for its own preview

The Wave show's preview in `scripts/wave_show.py` and the GitHub Pages gallery SHALL be produced by driving the Wave C++ source against a `MockStrip` via the show simulator binary (`[env:native_show_sim]`). There SHALL NOT be a separate hand-written Python re-implementation of the Wave algorithm in the repository. Any change to `src/show/Wave.cpp` is reflected in the next gallery regeneration with no further code changes.

#### Scenario: Gallery Wave preview matches the device

- **WHEN** the gallery is regenerated
- **THEN** `docs/show_previews/Wave.png` is produced by the simulator binary, not by a Python port
- **THEN** every pixel in `Wave.png` equals the value the device would write to the corresponding LED at the corresponding iteration

#### Scenario: No parallel Python port exists

- **WHEN** the repository is searched for a Python implementation of the Wave algorithm
- **THEN** no match is found outside `scripts/wave_show.py`'s subprocess invocation of the simulator binary
