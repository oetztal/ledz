## Context

The Wave show (`src/show/Wave.cpp`) currently produces a single sine wave from a *fixed* source at position 0, with exponential decay toward the far end. The pattern is short-lived: half the strip is dark within one cycle, and the entire system is periodic and predictable. There is no reflection, no interference, and no sense of the strip as a resonant space.

This change refreshes Wave by replacing the fixed source with an *oscillating* source. Because the source moves, two things happen for free:

- **Reflection**: when the source reaches an end and reverses direction, it naturally emits waves going the other way.
- **Interference**: at any moment, waves are being emitted in the current direction while previous waves from earlier source positions are still propagating in the opposite direction, so the strip fills with overlapping wavefronts that sum.

This refresh is intentionally minimal: same parameter names where possible, same computational cost, no new UI, no new persistence concerns. The geometry changes; the contract mostly doesn't.

### Constraints

- **No PSRAM**: Adafruit QT Py ESP32-S3, ~200KB available RAM. Cannot introduce allocations inside `execute()`.
- **100Hz LED task**: 10ms budget per frame on Core 1. Must remain single-pass over pixels with bounded math.
- **JSON parameter contract**: backwards compatibility with NVS-stored configs is desirable — missing fields fall through `ArduinoJson`'s `|` operator to defaults.
- **ShowFactory registration**: parameter parsing is the only place JSON keys are read for Wave; this is where dropped or repurposed parameters are handled.

## Goals / Non-Goals

**Goals:**

- Replace Wave's geometry with a bouncing-source model that produces reflection and interference naturally.
- Keep all three core parameters (`decay_rate`, `brightness_frequency`, `wavelength`) accepting the same JSON keys and default values.
- Preserve the same computational profile: one `sin`, one `exp`, one `wheel` per pixel per iteration.
- Document the dropped `wave_speed` parameter as a soft breaking change with graceful fallback.
- Make the design extensible so the future possibilities listed in the proposal can each be added with minimal additional structure.

**Non-Goals:**

- Adding new parameters to the web UI.
- Changing other shows.
- Changing persistence or show switching mechanics.
- Implementing any of the future possibilities (detune mode, second source, envelope LFO, palette refactor, source-intensity fade, triangle source) — these are listed in `proposal.md` and intentionally out of scope here.
- Replacing the `wheel()` color cycle with a different palette scheme.
- Refactoring Wave into a base class that other shows inherit from.

## Decisions

### Decision 1: Source motion shape is cosine

The source position oscillates between 0 and `N-1` following:

```
source_pos(t) = (N - 1) * 0.5 * (1 - cos(t * 2π * brightness_frequency))
```

- At `t = 0`: `source_pos = 0`
- At `t = 1/(2·brightness_frequency)`: `source_pos = N-1`
- Velocity is continuous (no jolt at the bounce)
- Source lingers at the ends (slower motion there)

**Alternatives considered:**

- *Triangle wave*: constant source speed, but velocity discontinuity at each bounce produces a visible jolt in the wavefronts. Rejected for default; trivially swappable later as a `source_shape` parameter if users prefer the energetic feel.
- *Random walk*: chaotic, but loses the harmonic identity we want to preserve for Path A. Reserved for the "detune / moiré" future possibility.
- *Stationary source at multiple positions*: doesn't naturally produce reflection — each source emits independently rather than the previous source's emission continuing to propagate.

### Decision 2: Decay is distance-from-source, not distance-from-end

```
envelope(i, t) = exp(-decay_rate * |i - source_pos(t)| / N)
```

This replaces the old `exp(-decay_rate * i / N)`, which assumed a fixed source at the left end. With an oscillating source, the bright spot follows the source — wherever the source is, that region is brightest, fading symmetrically toward both ends.

This means the *whole* strip is meaningfully lit (modulated by source position), rather than the left half dominating and the right half being dim.

### Decision 3: Wave amplitude is signed, brightness takes absolute value

```
phase(i, t) = (i - source_pos(t)) / wavelength
wave(i, t)  = sin(2π * phase(i, t))
brightness(i, t) = source_brightness(t) * envelope(i, t) * |wave(i, t)|
```

Using `fabs(wave)` keeps the strip positive-valued and lit. Using signed wave directly would produce dark regions wherever the sine goes negative — half the strip dark at any moment, defeating the point.

**Why this choice future-proofs:** when the second-source future possibility lands, the math becomes `|wave_a + wave_b|`, which is the textbook formula for true interference (nodes where waves cancel, antinodes where they reinforce). With Path A's single source, `|wave_a|` is the same as `|wave_a|` — no change. The code structure doesn't need to be rewritten to add a second source.

### Decision 4: `wave_speed` parameter is dropped

The old `wave_speed` controlled how fast the wavefront traveled out from position 0. In the new model, wave speed is implicit in the source motion. The JSON field is silently ignored if sent (the constructor no longer reads it; missing fields fall through `|` to defaults). NVS configs that contain `wave_speed` continue to load — the field is just not consumed.

**Alternatives considered:**

- *Repurpose `wave_speed` as source motion speed*: rejected — would conflict semantically with `brightness_frequency`, which already controls how fast the source moves.
- *Keep `wave_speed` as a no-op for one release*: rejected — silent dead knobs are worse than clean removal. Anyone scripting the API can simply stop sending the field.

### Decision 5: Default parameter values stay the same

| Parameter | Old default | New default | New meaning |
|---|---|---|---|
| `decay_rate` | `2.0` | `2.0` | exp decay per unit distance from oscillating source |
| `brightness_frequency` | `0.1` | `0.1` | source bounce frequency (cycles per second) |
| `wavelength` | `6.0` | `6.0` | wave wavelength in pixels |

Defaults are unchanged because they already produce a visually pleasant result in the new model — the strip fills with overlapping waves, decay is gentle enough to keep the far end visible, wavelength gives roughly 3-5 wavefronts visible at any moment on a 60-pixel strip.

The only thing to validate during implementation is that `decay_rate = 2.0` doesn't over-darken at typical strip lengths (e.g., 30, 60, 144 pixels). If it does, the default can be lowered without affecting the spec — defaults are not normative.

### Decision 6: Wave constructor signature

```cpp
Wave(float decay_rate = 2.0f,
     float brightness_frequency = 0.1f,
     float wavelength = 6.0f);
```

Three parameters, same order and types as before, just dropping the first. `ShowFactory.cpp` updates accordingly: only parse three fields.

### Decision 7: Test coverage is smoke + spot-checks, not exhaustive

`test/test_shows/test_shows.cpp` currently has zero Wave tests. Adding exhaustive behavioral tests (e.g., "at iteration N, pixel K has hue H") would be brittle — the spec allows wide parameter ranges and is intentionally subjective about visual quality. Instead:

- **Smoke test**: construct `Wave` with default and non-default parameters, execute for several iterations against `MockStrip`, assert no crash.
- **Coverage test**: at a moment when source is mid-strip, assert that pixels on both sides of the source are non-black (proves the symmetric decay works).
- **Parameter persistence test**: parse a known JSON, construct, assert field values flowed through (matches the existing Rainbow test pattern).

This is enough to catch regressions without locking the implementation to a specific visual signature.

## Risks / Trade-offs

- **Hot-spot at strip ends** → cosine source lingers at each end, so brightness concentrates there briefly. *Mitigation*: this is documented as a future possibility (`source-intensity fade near ends` in the proposal). If Path A in practice shows the hot-spot is distracting, that future change ships; otherwise leave it.

- **Existing JSON configs with `wave_speed`** → field is silently ignored, which means a user who set `wave_speed: 5.0` expecting fast waves sees default behavior instead. *Mitigation*: documented as soft breaking in the proposal; default behavior is more interesting than the old behavior anyway. NVS-stored configs don't error.

- **Color cycle still uses `wheel()` based on emission time** → every wave is rainbow-painted, which the user may still find visually busy after Path A. *Mitigation*: the palette refactor is a separate, independent change in the future possibilities. Path A doesn't claim to fix palette, only geometry.

- **Possible regression at very short strip lengths (<10 pixels)** → wavelength of 6 means roughly 1-2 wavefronts fit; the bouncing source still works but interference is less interesting. *Mitigation*: users can lower `wavelength` themselves; this is not the common case (typical strips are 30-300 pixels).

- **CPU profile is unchanged but code path is restructured** → risk of accidentally introducing per-pixel allocation, division, or other expensive ops. *Mitigation*: implementation review against the existing `Rainbow.cpp` and `Fire.cpp` as references for the per-pixel-loop-with-no-allocation pattern; smoke test will catch crashes but not perf regressions, so the change should be benchmarked if there is doubt.

- **`fabs(wave)` discards the negative half of each sine** → the spec does not preserve the destructive interference effect within a single wave (only the sum across sources does, when that lands). *Mitigation*: this is a deliberate aesthetic choice — Path A wants the strip lit, not half-dark. When a second source is added, the sum's sign is preserved by taking `|wave_a + wave_b|`, restoring true interference at the multi-source level.

## Open Questions

None blocking implementation. The following are design observations that may inform future changes but do not require resolution for this one:

- Should `decay_rate` be reinterpreted as decay *per wavelength* rather than per strip length, so the visual decay is independent of `N`? (Out of scope; current behavior is fine for default strips.)
- Should the source position be exposed as a separate parameter for users who want a *fixed* source (i.e., to recreate the old behavior intentionally)? (No — that is a different show; users wanting that should just select Wave with custom `decay_rate`.)
- Should there be a `source_shape: cosine | triangle` parameter exposed now, or wait until users ask? (Recommendation: wait. See future possibility 6 in the proposal.)
