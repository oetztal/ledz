# echo-show Specification

## Purpose

The Echo show moves several coloured particles back and forth along the strip and uses feedback decay so each one leaves a long, fading comet trail.

## Requirements

### Requirement: Echo particles bounce between the ends

The Echo show SHALL move `particles` emitters along the strip. A particle reaching either end SHALL reverse direction and stay within `[0, N - 1]`. Starting positions and directions SHALL come from the show random source, so a fixed seed yields identical output.

#### Scenario: Particles stay on the strip

- **WHEN** Echo runs with any valid parameters for 1000 iterations
- **THEN** every particle position stays within `[0, N - 1]`

#### Scenario: Deterministic with a seed

- **WHEN** Echo runs twice with the same parameters and the same random seed
- **THEN** both runs produce identical pixel output

### Requirement: Echo particles move at different speeds

Particle `j` of `P` SHALL move at `speed * (0.6 + 0.8 * j / (P - 1))` pixels per iteration, or at `speed` when `P == 1`. The particles therefore travel at speeds spread evenly between `0.6` and `1.4` times `speed`, and their trails cross at varying angles.

#### Scenario: Particles move at different speeds

- **WHEN** Echo runs with `particles == 3` and `speed == 1`
- **THEN** the particles move `0.6`, `1.0` and `1.4` pixels per iteration

### Requirement: Echo particles have distinct hues

Particle `j` of `P` SHALL have hue `(j * 256 / P + drift) mod 256`, rendered through `wheel()`, where `drift` advances by `hue_speed` steps per iteration. Particles SHALL therefore always be evenly spread around the colour wheel.

#### Scenario: Two particles differ in colour

- **WHEN** Echo runs with `particles == 2` and `hue_speed == 0`
- **THEN** the two particles' emitted colours are `wheel(0)` and `wheel(128)`

### Requirement: Echo trails are continuous at any speed

Each iteration a particle SHALL emit light along the whole path it travelled in that iteration, including any reflection at an end, in anti-aliased steps of at most `0.25` pixels whose intensities add up to the distance travelled. Every pixel a particle passes SHALL therefore receive about one full unit of light, whatever the speed, so trails have no stripes.

#### Scenario: No stripes at fractional speeds

- **WHEN** Echo runs with `particles == 1`, `speed == 0.8`, `decay == 1`, `hue_speed == 0` until the particle has travelled 20 pixels without reaching an end
- **THEN** every pixel more than one pixel behind the particle along that path has every non-zero channel of `wheel(0)` at `250` or more

### Requirement: Echo particles leave decaying trails

The Echo show SHALL render through feedback with its `decay` parameter as the decay factor and no zoom, scroll, warp or mirror. Pixels a particle has passed SHALL stay lit and fade over later iterations.

#### Scenario: Trail behind a particle

- **WHEN** Echo runs with `particles == 1`, `speed == 1`, `decay == 0.9` for 20 iterations on a 60-pixel strip
- **THEN** at least 5 pixels are non-black, with brightness falling off with distance behind the particle

### Requirement: Echo accepts parameters

The Echo show SHALL accept `particles` (integer, default `4`, clamped to `[1, 8]`), `speed` (float, default `1.0`, clamped to `[0.05, 4.0]`), `decay` (float, default `0.96`, clamped to `[0.0, 1.0]`), and `hue_speed` (float, default `0.5`, clamped to `[0.0, 16.0]`). Missing keys SHALL take their defaults; out-of-range values SHALL be clamped without error.

#### Scenario: Defaults from empty params

- **WHEN** Echo is created with `params_json == "{}"`
- **THEN** it has `particles=4`, `speed=1.0`, `decay=0.96`, `hue_speed=0.5`

#### Scenario: Particle count clamped

- **WHEN** Echo is created with `params_json == "{\"particles\":50}"`
- **THEN** it has `particles=8`

#### Scenario: Selectable in the web UI

- **WHEN** a user selects Echo in the show dropdown
- **THEN** the Echo parameter section is shown with inputs for every parameter and an Apply button that POSTs them to `/api/show`, and the show does not start until Apply is pressed
