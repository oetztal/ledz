# Spec Delta

## Purpose

Defines the frame-to-frame feedback behaviour shared by feedback-based shows: the previous frame is remapped, decayed, and combined with newly emitted light, producing trails and tunnels on a 1D strip.

## ADDED Requirements

### Requirement: Feedback state persists across frames

A feedback-based show SHALL keep a per-pixel RGB intensity state between iterations. Each iteration SHALL derive the new state from the previous state, so light emitted in one iteration remains visible, transformed and decayed, in later iterations.

#### Scenario: Emitted light persists after emission stops

- **WHEN** a pixel is lit at full intensity in one iteration and nothing is emitted in the following iteration, with decay `0.9` and identity transform
- **THEN** that pixel's output in the following iteration is `round(255 * 0.9)` = `230` in each channel that was lit

#### Scenario: State starts black

- **WHEN** a feedback-based show executes its first iteration without emitting light
- **THEN** every pixel is black

### Requirement: Decay applies once per iteration

Before new light is added, every channel of the remapped previous state SHALL be multiplied by the decay factor, a value in `[0, 1]`. Decay `0` SHALL clear the previous frame entirely; decay `1` SHALL keep it unattenuated.

#### Scenario: Geometric fade

- **WHEN** a pixel holds intensity `1.0` and `k` iterations pass with decay `d` and no emission at that pixel under the identity transform
- **THEN** the pixel's intensity is `d^k`

#### Scenario: Zero decay leaves only fresh emission

- **WHEN** decay is `0`
- **THEN** each iteration's output equals only the light emitted in that iteration

### Requirement: Faded light reaches true black

Intensities SHALL convert to 8-bit output channels by rounding to the nearest integer, after clamping to `[0, 1]` and scaling by 255. Any trail with decay below `1` SHALL therefore become fully black in a finite number of iterations, never leaving a stuck dim glow.

#### Scenario: Trail fully fades

- **WHEN** a pixel is lit once at full intensity and then decays with factor `0.9` without further emission
- **THEN** its output becomes `0` in all channels within 60 iterations and stays `0`

### Requirement: Emission adds light with saturation

Light emitted by the show in an iteration SHALL be added to the decayed state per channel. A channel's state SHALL be clamped to at most `1.0`, so overlapping emitters brighten up to full intensity but never overflow or wrap around.

#### Scenario: Overlapping emission saturates

- **WHEN** two emissions of intensity `0.7` land on the same channel of the same pixel in one iteration
- **THEN** that channel's output is `255`

### Requirement: Zoom remaps about the strip centre

With zoom factor `z`, each pixel at position `x` SHALL take the previous state sampled at `c + (x - c) * z`, where `c = (N - 1) / 2` and `N` is the strip length. A zoom below 1 SHALL make content move outward from the centre; a zoom above 1 SHALL make it move inward. Sampling between pixels SHALL interpolate linearly.

#### Scenario: Zoom below one moves content outward

- **WHEN** a lit pixel sits off-centre and several iterations run with zoom `0.9` and decay `1`
- **THEN** the brightest point moves strictly farther from the centre each iteration

#### Scenario: Zoom of one is the identity

- **WHEN** zoom is `1` and scroll and warp are `0` and mirror is off
- **THEN** with decay `1` and no emission, the output is identical to the previous iteration

### Requirement: Scroll shifts content with wrap-around

With scroll `v` (pixels per iteration, may be fractional or negative), the sample position SHALL be offset by `-v`, so content moves `v` pixels toward higher indices per iteration. Scroll SHALL wrap around the strip ends rather than lose content.

#### Scenario: Content wraps

- **WHEN** a single lit pixel sits at index `N - 1` and one iteration runs with scroll `1`, decay `1`, zoom `1`
- **THEN** the lit pixel appears at index `0`

### Requirement: Warp wobbles the sample position

With warp amplitude `a` (pixels), spatial frequency `k` and phase speed `w`, the sample position SHALL be offset by `a * sin(k * x + w * t)`, where `t` is the iteration count. Warp amplitude `0` SHALL have no effect.

#### Scenario: Warp of zero changes nothing

- **WHEN** warp amplitude is `0`
- **THEN** output is identical to the same configuration without a warp term

### Requirement: Push moves content outward at a constant speed

With push `p` (pixels per iteration, `p >= 0`), the sample position of a pixel on either side of the centre `c` SHALL be offset by `p` toward the centre, after zoom and before warp, so content moves `p` pixels per iteration away from the centre in addition to any zoom. Push `0` SHALL have no effect. Unlike zoom, push moves content near the centre as fast as content near the ends, so light emitted at the centre leaves it immediately.

#### Scenario: Push moves a pixel outward

- **WHEN** a single lit pixel sits at `c + 5` on a strip with odd `N`, and one iteration runs with push `1`, zoom `1`, decay `1`
- **THEN** the lit pixel appears at `c + 6`

#### Scenario: Push mirrors on both sides

- **WHEN** a single lit pixel sits at `c - 5` on a strip with odd `N`, and one iteration runs with push `1`, zoom `1`, decay `1`
- **THEN** the lit pixel appears at `c - 6`

#### Scenario: Push of zero changes nothing

- **WHEN** push is `0`
- **THEN** output is identical to the same configuration without a push term

### Requirement: Push empties the centre pixel(s)

When push is above `0`, the pixel(s) nearest the centre `c` (the single pixel at `c` for odd `N`, or the pair at `c ± 0.5` for even `N`) SHALL sample black, because all content flows away from them and nothing flows in. Light emitted there is therefore shown for one iteration and then carried outward, rather than held and accumulated.

#### Scenario: Centre pixel does not accumulate

- **WHEN** the centre pixel of a strip with odd `N` is lit and one iteration runs with push `1`, decay `1` and no emission
- **THEN** the centre pixel is black

#### Scenario: Even-length centre pair does not accumulate

- **WHEN** the two centre pixels of a strip with even `N` are lit and one iteration runs with push `1`, decay `1` and no emission
- **THEN** both centre pixels are black

### Requirement: Mirror fold makes the strip symmetric

When mirror is enabled, each pixel SHALL sample the previous state at its distance from the centre, folded onto the lower half, so the resulting frame is symmetric about the strip centre.

#### Scenario: Mirrored output is symmetric

- **WHEN** mirror is enabled and any emission pattern runs for at least one iteration
- **THEN** pixels `i` and `N - 1 - i` have equal colour after that iteration's remap and before the next emission

### Requirement: Out-of-range samples are black

When scroll is `0` and zoom or warp produces a sample position outside `[0, N - 1]`, the sampled value SHALL be black, so content leaving the strip ends disappears rather than smearing the edge pixel. When scroll is non-zero, sample positions SHALL wrap modulo `N` instead.

The zoom centre `c` is a fixed point of the remap, so light on the centre pixels (the one or two pixels nearest `c`) keeps sampling itself and does not leave the strip under zoom alone.

#### Scenario: Content exits at the ends

- **WHEN** zoom is below `1`, nothing is emitted, decay is `1`, and the centre pixels start black
- **THEN** after enough iterations every pixel is black

### Requirement: Feedback memory is bounded and released

A feedback-based show SHALL use at most 24 bytes of state per LED. Its state SHALL size itself to the strip length on the first iteration and whenever the strip length changes, and SHALL be released when the show is replaced.

#### Scenario: Strip length change

- **WHEN** a feedback-based show runs on a strip whose length changes between iterations
- **THEN** the show continues to render without error, covering every pixel of the new length
