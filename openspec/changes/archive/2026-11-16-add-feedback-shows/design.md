# Design

## Context

Shows implement `Show::execute(Strip::Strip&, Iteration)` and run on Core 1
at about 100 Hz. Shows with per-pixel state, such as Fire's `FireState`, keep
`std::vector<float>` buffers sized to `strip.length()`. `Strip::Color` is a
packed `uint32_t`; `Support::wheel(float)` maps a hue position to a colour;
`Support::Random` / `Support::randomSeed()` provide a seedable PRNG, which the
simulator's `--seed` relies on. Default parameters come from
`scripts/show_variants.json` through the generated header, and the web UI's
parameter sections live in `data/control.html`. The strip can hold up to 1000
LEDs (`/api/settings/device` validation), and RAM is limited because there is
no PSRAM.

## Goals / Non-Goals

**Goals:**
- One small, show-agnostic feedback class that Tunnel, Echo and later
  feedback shows (Plasma Smear, Kaleidoscope, …) all reuse.
- Deterministic output, so the native tests and the simulator gallery are
  stable.
- Each frame costs O(N) with no heap allocation after the first frame.

**Non-Goals:**
- Audio reactivity, beat detection, or crossfading between shows.
- Blur and other multi-tap filters (they could be added to the engine later).
- Exposing the raw feedback transform as a user-configurable "preset language".

## Decisions

### `Support::Feedback` owns the buffers and the shows only emit

```cpp
namespace Support {
    struct FeedbackTransform {
        float zoom = 1.0f;       // about centre; <1 expands outward
        float scroll = 0.0f;     // px/iteration, wraps when non-zero
        float warp_amp = 0.0f;   // px
        float warp_freq = 0.3f;  // rad/px
        float warp_speed = 0.05f;// rad/iteration
        bool mirror = false;
        float push = 0.0f;       // px/iteration outward from centre
    };
    class Feedback {
    public:
        void begin(Strip::PixelIndex n, const FeedbackTransform&, float decay, uint64_t t); // remap+decay
        void add(float pos, Strip::Color c, float intensity = 1.0f); // anti-aliased, saturating
        void addPixel(Strip::PixelIndex i, Strip::Color c, float intensity = 1.0f);
        void render(Strip::Strip&) const;                             // round to 8-bit
    };
}
```

Each frame a show calls `begin`, then `add` / `addPixel`, then `render`.
`begin` resizes the buffers lazily when `n` changes; the spec requires the
state to follow the strip length. Keeping the class in `src/support/` puts it
in the native build and the simulator automatically.

*Alternative considered:* a `FeedbackShow` base class with a virtual
`emit()`. It was rejected because composition keeps `Show` unchanged and
lets a show use two feedback layers, or none, without reworking a class
hierarchy.

### Two float RGB buffers (front/back), swapped each frame

The remap reads arbitrary source positions (warp, zoom), so it cannot run
in place without direction-dependent ordering that warp would break. Two
`std::vector<float>` of `3*N` cost 24 B/LED: 7.2 KB at 300 LEDs and 24 KB at
1000. Swapping the vectors with `std::swap` costs no allocation.

*Alternative considered:* 8.8 fixed-point `uint16_t` buffers at 12 B/LED.
They would halve the memory, but the arithmetic is less readable and the
precision gain is marginal. We keep that option in reserve if the memory
risk below materialises; the public API would not change.

### Sample-position pipeline

For destination `x`: when mirror is on, `x ← (x > c) ? 2c − x : x`. Then
`s = c + (x − c)·zoom − sign(x − c)·push − scroll + warp_amp·sin(warp_freq·x + warp_speed·t)`,
followed by a linear-interpolation sample. When scroll is non-zero, `s` wraps
modulo N; otherwise samples outside the strip read as black.
Because each frame remaps the previous frame, the per-frame `scroll` offset
is enough; nothing accumulates. Mirror is applied to
the destination index, which makes symmetry hold by construction.

### Radial push

Zoom alone moves content in proportion to its distance from the centre, so
light emitted at the centre barely moves for its first frames: successive
emissions pile up there and the linear interpolation smears them into one
glow, with no visible rings (found on the device and in the gallery). `push`
adds a constant outward offset on each side of the centre, so freshly
emitted light leaves the centre at `push` px/iteration and zoom then
accelerates it toward the ends. Push is applied to the folded destination
index, so mirror symmetry still holds by construction. On odd-length strips
the centre pixel is a fixed point that push cannot move; on even-length strips
the two middle pixels sample each other. Under continuous emission either
would keep and re-add light and saturate (a native test caught the odd case and
the 300-pixel gallery showed the even pair). With push above 0 the centre
pixel(s), `|x − c| <= 0.5`, therefore sample black: under outward flow nothing
flows into them.

### Tunnel pulses

A continuously emitting centre produces smooth colour bands rather than
rings. Tunnel modulates the emitted intensity with a sharpened cosine
(`0.08 + 0.92·((1 + cos(2π·f / pulse)) / 2)^6`), so it flashes once every
`pulse` iterations and glows dimly in between. Each flash becomes a ring
that push and zoom carry outward, and the dark gaps between rings give the
contrast the continuous emission lacked. `pulse = 0` restores continuous
emission. `f` is the show's own frame counter, like the hue accumulator.

### Tunnel sway

With a fixed centre every ring is the same symmetric chevron at the same
interval, which reads as monotonous in the gallery. Tunnel therefore moves
the transform centre (`FeedbackTransform::center`) and the emission point
along `sway·half·(sin(0.015 f) + 0.5 sin(0.037 f + 1)) / 1.5`. Two
incommensurate sines (about 4.2 s and 1.7 s at 100 Hz) keep the drift from
looping visibly, and the faster period lets a 300-iteration preview show
more than one bend. The mirror fold is only used when both warp and sway are
`0`. The off-pixel centre exposed a push artefact: pixels nearer the centre
than the push sampled the opposite side, interleaving both halves into a
comb (mirroring had hidden it). Push therefore clamps the sample at the
centre. An extra complementary-colour "wall" glow between rings was tried
and dropped: on the strip it shows as fast colour flicker.

### Echo speeds and swept emission

Equal particle speeds make every trail parallel, a regular lattice. Particle
`j` instead moves at `speed·(0.6 + 0.8·j / (P − 1))`, so trails cross at
varying angles; the factors are deterministic, so a seed still fixes the
output. Point emission at the end position leaves stripes whenever a
particle covers a fractional number of pixels per frame (pixels receive
light on differing numbers of frames). Echo therefore advances each
particle in sub-steps of at most 0.25 px, reflecting at the ends per
sub-step, and emits `step` intensity at each. A numeric check of the
deposit showed every passed pixel within about 1% of full intensity for
speeds from 0.05 to 5.6 px/iteration (0.5 px steps left up to 4% error,
visible as 245/255).

### Anti-aliased emission

`add(pos, …)` splits intensity between `floor(pos)` and `floor(pos)+1` by
their fractional weights. Without the split, Echo particles at
sub-pixel speeds (default 0.5 px/iteration) would visibly step from pixel to
pixel. Channels saturate at 1.0 on add.

### Hue accumulation

Tunnel and Echo keep the hue as a float accumulator wrapped with `fmodf(…,
256.0f)` and pass it to `Support::wheel`, matching the existing shows.

### Echo state and randomness

Echo stores `particles` as a small `std::vector<{float pos; float dir; float speed;}>`
(at most 8). The vector is initialised on the first `execute` from a
`Support::Random` seeded with `Support::randomSeed()`, because the strip
length is only known then. When a position passes an end, it is reflected
(`pos = −pos` or `2(N−1) − pos`) and `dir` flips; the emission for that
sub-step is split at the end pixel so the turn-around is not skipped.

### Parameter clamping in the show constructors

The constructors clamp values to the ranges in the specs. The factory
lambdas read JSON with the existing `doc["k"] | default` pattern, and their
defaults come from the generated `show_variants.h`. That keeps the
canonical-show-config rule that constructors have no default arguments.

### UI wiring follows the Wave pattern

Each show gets a `params-section` (`tunnelParams`, `echoParams`) with
`data-variants-for`, a case in `updateParameterVisibility()`, an entry in
`showsWithParams`, `applyTunnelParams()` / `applyEchoParams()`, and a case in
the load-params switch. Curated presets come from the manifest:
- **Tunnel**: `hyperspace` (faster push, shorter pulse period, faster hue)
  and `liquid` (continuous emission with a gentle warp). Exact values live in
  `scripts/show_variants.json`.
- **Echo**: `swarm` (8 particles, shorter trails) and `ghost` (1 slow
  particle, very long trail).

### Default and preset tuning

The defaults and presets were tuned against the 300-pixel gallery renders.
With a zoom near 1, content close to the centre moves less than a pixel per
frame, so a slow zoom smears into a dim glow instead of visible rings: the
Tunnel default uses zoom 0.97, decay 0.93 and hue_speed 2.5 so distinct colour
bands reach the ends. The warp period (2π / 0.05 ≈ 126 iterations) must not line
up with the hue period (256 / hue_speed): at hue_speed 2 they lock and every
wobble repeats the same colour, so `liquid` uses 1.3. Retuning alone did not
make Tunnel lively enough on the device, which led to the radial push and
pulses above.

## Risks / Trade-offs

- [24 KB at 1000 LEDs on top of the ~59 KB baseline] → The memory is freed
  when the show changes, and only one show is alive at a time. A native test
  asserts the buffer sizes are `3*N` each. Fall back to 8.8 fixed point if
  heap fragmentation shows up in testing.
- [Allocation on the LED task when the strip length changes] → It happens
  once per resize, not per frame. On allocation failure, `begin()` keeps the
  old size and logs an `E` once.
- [Float `sinf` per pixel per frame for warp] → `sinf` is skipped when
  `warp_amp == 0` (Echo, default Tunnel). At 1000 LEDs × 100 Hz that is
  100k `sinf`/s on the ESP32-S3 FPU when warp is on. This is acceptable, and
  we will measure it with the existing iteration timing log.
- [Spec uses "about 100 Hz" while Wave assumes 0.05 s/iteration] → Feedback
  parameters are defined per iteration, not per second, so they don't depend
  on that discrepancy.
