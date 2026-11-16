# Spec Delta

## MODIFIED Requirements

### Requirement: Tunnel pulses its emission

When `pulse > 0`, the Tunnel show SHALL scale its emitted intensity by
`0.08 + 0.92 * ((1 + cos(2π * d / pulse)) / 2)^6`, where `d` is the accumulated
outward travel in pixels (increasing by `push` each iteration from `0`). The
centre therefore flashes once every `pulse` pixels of travel and glows dimly in
between, and each flash becomes a distinct ring of constant spatial thickness.
When `pulse == 0` the intensity SHALL be `1` in every iteration. When
`push == 0`, `d` SHALL be replaced by the iteration count `f`.

#### Scenario: Pulses form rings

- **WHEN** Tunnel runs with `push == 1`, `pulse == 20` and the centre pixel's brightness is sampled at iterations `0`, `10` and `20`
- **THEN** it is brighter at iterations `0` and `20` than at iteration `10`

#### Scenario: Continuous emission without pulse

- **WHEN** Tunnel runs with `pulse == 0`
- **THEN** the centre pixel has full intensity in every iteration

#### Scenario: Pulse phase follows distance

- **WHEN** Tunnel is constructed with `push == 0.5` and `pulse == 20`
- **THEN** `pulseIntensity(0)`, `pulseIntensity(20)` and `pulseIntensity(40)` are `1`, and `pulseIntensity(10)` and `pulseIntensity(30)` are `0.08`

#### Scenario: Slow push keeps the ring spacing

- **WHEN** Tunnel runs with `zoom == 1`, `decay == 0.97`, `push == 0.25`, `pulse == 20`, `hue_speed == 0` and `sway == 0` on a strip long enough for several rings
- **THEN** the ring spacing matches the same Tunnel at `push == 1` after equal accumulated travel

#### Scenario: Zero push keeps the frame-driven pulse

- **WHEN** Tunnel runs with `push == 0`, `pulse == 20`, `hue_speed == 0` and `sway == 0` on a 61-pixel strip
- **THEN** the centre pixel is dimmer at iteration `10` than at iterations `0` and `20`

### Requirement: Tunnel rings stream outward

The Tunnel show SHALL render through feedback with its `zoom` parameter as the
zoom factor and its `push` parameter as the push. The `decay` parameter SHALL be
the brightness kept per pixel of outward travel, so the per-iteration decay
passed to feedback SHALL be `decay ^ push` when `push > 0`, and SHALL be `decay`
unchanged when `push == 0`. With the default parameters, colour emitted at the
centre SHALL reach both strip ends, and the pixel distance a ring travels before
it fades SHALL be independent of `push`.

#### Scenario: Light reaches both ends with defaults

- **WHEN** Tunnel runs with default parameters on a 60-pixel strip for 200 iterations
- **THEN** pixel `0` and pixel `59` are each non-black in at least one of those iterations

#### Scenario: Slow push still reaches both ends

- **WHEN** Tunnel runs with `decay == 0.97`, `push == 0.25`, `pulse == 20` and `sway == 0` on a 60-pixel strip for 400 iterations
- **THEN** pixel `0` and pixel `59` are each non-black in at least one of those iterations

#### Scenario: Decay is per pixel, not per frame

- **WHEN** Tunnel is constructed with `decay == 0.97` and `push == 0.5`
- **THEN** its per-iteration decay factor is `0.97 ^ 0.5`

#### Scenario: Zero push keeps the per-frame decay

- **WHEN** Tunnel is constructed with `decay == 0.9` and `push == 0`
- **THEN** its per-iteration decay factor is `0.9`

#### Scenario: Output is symmetric without warp or sway

- **WHEN** Tunnel runs with `warp == 0` and `sway == 0`
- **THEN** pixels `i` and `N - 1 - i` have equal colour at every iteration
