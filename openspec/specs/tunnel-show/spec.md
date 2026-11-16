# tunnel-show Specification

## Purpose

The Tunnel show emits a pulsing, hue-cycling light at the strip centre and pushes and zooms the previous frame outward, so distinct rings of colour fly from the middle toward both ends like a MilkDrop tunnel.

## Requirements

### Requirement: Tunnel emits at the centre with cycling hue

Each iteration the Tunnel show SHALL emit light at the strip centre (the one or two pixels nearest `(N - 1) / 2`), with colour `wheel(hue)`. The hue SHALL advance by `hue_speed` wheel steps per iteration and wrap modulo 256.

#### Scenario: Centre is lit from the first iteration

- **WHEN** Tunnel executes its first iteration on a strip of any length `N >= 1`
- **THEN** the centre pixel is non-black

#### Scenario: Hue changes over time

- **WHEN** Tunnel runs with `hue_speed > 0` and the centre pixel's colour is sampled 32 iterations apart
- **THEN** the two colours differ

### Requirement: Tunnel pulses its emission

When `pulse > 0`, the Tunnel show SHALL scale its emitted intensity by `0.08 + 0.92 * ((1 + cos(2π * f / pulse)) / 2)^6`, where `f` counts the show's iterations from `0`. The centre therefore flashes once every `pulse` iterations and glows dimly in between, and each flash becomes a distinct ring. When `pulse == 0` the intensity SHALL be `1` in every iteration.

#### Scenario: Pulses form rings

- **WHEN** Tunnel runs with `pulse == 20` and the centre pixel's brightness is sampled at iterations `0`, `10` and `20`
- **THEN** it is brighter at iterations `0` and `20` than at iteration `10`

#### Scenario: Continuous emission without pulse

- **WHEN** Tunnel runs with `pulse == 0`
- **THEN** the centre pixel has full intensity in every iteration

### Requirement: Tunnel rings stream outward

The Tunnel show SHALL render through feedback with its `zoom` parameter as the zoom factor, its `push` parameter as the push, and its `decay` parameter as the decay factor. With the default parameters, colour emitted at the centre SHALL reach both strip ends.

#### Scenario: Light reaches both ends with defaults

- **WHEN** Tunnel runs with default parameters on a 60-pixel strip for 200 iterations
- **THEN** pixel `0` and pixel `59` are each non-black in at least one of those iterations

#### Scenario: Output is symmetric without warp or sway

- **WHEN** Tunnel runs with `warp == 0` and `sway == 0`
- **THEN** pixels `i` and `N - 1 - i` have equal colour at every iteration

### Requirement: Tunnel warp adds a wobble

When `warp > 0`, the Tunnel show SHALL apply the feedback warp with amplitude `warp` pixels, so rings bend over time instead of travelling in straight lines.

#### Scenario: Warp breaks symmetry

- **WHEN** Tunnel runs with `warp == 2.0` for 100 iterations on a 60-pixel strip
- **THEN** at least one iteration has pixels `i` and `N - 1 - i` with differing colour for some `i`

### Requirement: Tunnel sway drifts the vanishing point

The Tunnel show SHALL offset its centre (emission point and feedback transform centre) from the strip middle by `sway * ((N - 1) / 2) * (sin(0.015 f) + 0.5 sin(0.037 f + 1)) / 1.5` pixels, where `f` counts the show's iterations from `0`, so the rings curve as if the tunnel turned. An off-pixel centre SHALL get anti-aliased emission. With `sway == 0` the centre SHALL stay at the middle.

#### Scenario: Sway breaks symmetry

- **WHEN** Tunnel runs with `warp == 0` and `sway == 0.4` for 100 iterations on a 60-pixel strip
- **THEN** at least one iteration has pixels `i` and `N - 1 - i` with differing colour for some `i`

#### Scenario: Sway moves the brightest pixel

- **WHEN** Tunnel runs with `sway == 1.0`, `pulse == 0` and `push == 1` on a 61-pixel strip for 300 iterations
- **THEN** the brightest pixel is left of the middle in some iteration and right of it in another

### Requirement: Tunnel accepts parameters

The Tunnel show SHALL accept these parameters, each with a default and a clamp range: `zoom` (`0.97`, `[0.5, 1.0]`), `decay` (`0.97`, `[0.0, 1.0]`), `hue_speed` (`2.5`, `[0.0, 16.0]`), `warp` (`0.0`, `[0.0, 8.0]`), `push` (`1.0`, `[0.0, 4.0]`), `pulse` (integer, `20`, `[0, 200]`) and `sway` (`0.4`, `[0.0, 1.0]`); all but `pulse` are floats. Missing keys SHALL take their defaults; out-of-range values SHALL be clamped without error.

#### Scenario: Defaults from empty params

- **WHEN** Tunnel is created with `params_json == "{}"`
- **THEN** it has `zoom=0.97`, `decay=0.97`, `hue_speed=2.5`, `warp=0.0`, `push=1.0`, `pulse=20`, `sway=0.4`

#### Scenario: Out-of-range values are clamped

- **WHEN** Tunnel is created with `params_json == "{\"zoom\":3.0,\"decay\":-1}"`
- **THEN** it has `zoom=1.0` and `decay=0.0`

#### Scenario: Selectable in the web UI

- **WHEN** a user selects Tunnel in the show dropdown
- **THEN** the Tunnel parameter section is shown with inputs for every parameter and an Apply button that POSTs them to `/api/show`, and the show does not start until Apply is pressed
