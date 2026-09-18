## Context

`wave-interference` replaced the fixed-source Wave with a cosine-bouncing source. The shipped defaults and decay formula were the obvious "first cut":

- `decay_rate = 2.0` measured per unit strip length: `exp(-decay_rate * |i - source| / N)`.
- `brightness_frequency = 0.1`, `wavelength = 6.0`.

After watching the show on actual strips (30 to 144 pixels), two problems were visible:

1. **Under-lit far end.** With decay normalised to strip length, `exp(-2)` ≈ 0.13 at the far end on a 60-pixel strip, so the wavefront dies long before reaching the opposite tip. Visually the strip reads as "half the strip is dim" rather than as a resonant space.
2. **End hot-spot.** Cosine source has velocity zero at the extremes (`d/dt (1 - cos) = sin`, zero at `t = 0` and `t = π/f`). The source lingers at each end long enough to look like a stationary bright dot pinned to the wall.

This change is a spec-level follow-up that captures the implementation already in `src/show/Wave.cpp`: the decay formula and the defaults change, and a new end-fade behaviour is added.

### Constraints

- No PSRAM, ~200 KB available RAM. Cannot introduce per-frame allocations.
- 100 Hz LED task, 10 ms budget per frame on Core 1.
- Backwards compatibility: NVS-stored configs that omit fields continue to load via ArduinoJson's `|` operator.
- Computational profile stays unchanged at one `sin`, one `exp`, one `fabs`, one `wheel` per pixel per iteration.

## Goals / Non-Goals

**Goals:**

- Decouple `decay_rate` from strip length: the same value produces the same visual falloff on any strip length.
- Pick defaults that keep the strip meaningfully lit on typical lengths (30–144) and produce 3–5 distinct wavefronts visible at any moment.
- Eliminate the end hot-spot without changing the cosine source motion (which still has continuous velocity — no jolt at the bounce).
- Update the spec to match the code so the normative requirements stay accurate.

**Non-Goals:**

- Adding new parameters to the UI or to the JSON contract.
- Changing other shows.
- Replacing cosine source motion with a triangle wave (would reintroduce a velocity discontinuity at the bounce).
- Adding a second source or detune mode (those are listed in the archived `wave-interference/proposal.md` as future possibilities, out of scope here).

## Decisions

### Decision 1: Decay is per wavelength, not per strip length

```
envelope(i, t) = exp(-decay_rate * |i - source_pos(t)| / wavelength)
```

- Same `decay_rate` now means "how many wavelengths until brightness halves", regardless of `N`. Halving distance is `ln 2 / decay_rate` wavelengths in either direction.
- `decay_rate = 1.0` gives a half-brightness distance of `ln 2 ≈ 0.69` wavelengths ≈ 10 pixels at the default `wavelength = 15`. Reads as "bright source, gentle falloff" on a 60-pixel strip.
- Same `decay_rate = 1.0` on a 144-pixel strip still gives half-brightness at 10 pixels from the source; the rest of the strip still receives the wave, just dimmer.

**Alternatives considered:**

- *Per-pixel decay (`exp(-decay_rate * |i - source| / pixel_constant)`)*: same problem as before — needs a per-strip-length constant to feel right. Rejected.
- *Linear-in-distance falloff (no exp)*: simpler math but doesn't decay fast enough to keep the source region distinct from the rest of the strip. Rejected.
- *Per-strip-length decay kept, default tuned down to `0.5`*: works for 60-pixel strips but still leaves the spec coupled to strip length, which is what we're trying to escape. Rejected.

### Decision 2: New defaults `1.0 / 0.07 / 15.0`

| Parameter | Old | New | Rationale |
|---|---|---|---|
| `decay_rate` | 2.0 | 1.0 | With per-wavelength decay, 2.0 was too tight (half-brightness at 0.35 wavelengths ≈ 2 pixels — too narrow). 1.0 gives ~10 pixels of meaningful brightness on default strip. |
| `brightness_frequency` | 0.1 | 0.07 | Slower bounce: 14 s per full period (left→right→left) instead of 10 s. Lets the eye track individual wavefronts and gives the interference pattern time to develop. |
| `wavelength` | 6.0 | 15.0 | At wavelength 6 on a 60-pixel strip, ~10 wavefronts blur into rainbow mush. At wavelength 15, only 4 fit, and the eye reads them as distinct ripples. |

**Alternatives considered:**

- *More conservative defaults (2.0 / 0.05 / 20)*: too tame; the strip looks slow and undifferentiated.
- *Aggressive defaults (0.5 / 0.2 / 8)*: livelier but the wavefronts blur again and the far end of long strips is dim.
- *Keeping the old defaults and just changing the decay formula*: leaves the existing visual problems in place. Rejected.

### Decision 3: End-fade factor on source brightness

```
end_fade = 1 - 2 * |source_pos / (N - 1) - 0.5|
source_brightness = end_fade * (0.65 + 0.35 * sin(t))
```

- `end_fade = 1` when source is mid-strip, `0` at either end. Linear ramp in between.
- Multiplies the existing `0.65 + 0.35 * sin(t)` brightness oscillation, so the source naturally dims as it approaches the wall and re-emerges as it sweeps back toward the centre.
- The cosine source motion is preserved (continuous velocity at the bounce, no jolt); only the brightness scales. This keeps the spec consistent with the existing "Source motion has continuous velocity" requirement.

**Alternatives considered:**

- *Triangle-wave source motion*: fixes the lingering, but introduces a velocity discontinuity at the bounce. The bounce already produces a visible reflection; a jolt on top would look like a glitch. Rejected.
- *Bias exponent on the cosine (`(1 - cos)^k` with `k > 1`)*: claim was "keeps source near centre longer and sweeps ends quickly". On inspection the math is backwards — `k > 1` flattens the curve near the extremes, so the source actually lingers *more* at the ends with a `k > 1` bias, not less. Rejected.
- *Faster brightness_frequency*: gets the source past the ends faster, but the ends are still where the source spends most of its time (cosine velocity = 0 at extremes), and the strip looks rushed. Rejected.

### Decision 4: No new parameters

The three structural tweaks above change behaviour without adding UI knobs. The JSON contract stays at `decay_rate` / `brightness_frequency` / `wavelength`; `wave_speed` continues to be silently ignored.

## Risks / Trade-offs

- **Spec-vs-code drift was already a problem** — the previous change was archived with a decay formula that no longer matched the implementation. *Mitigation*: this change closes the loop by formalising the implementation in the spec.
- **Existing NVS configs will get new defaults on next reboot** — a user with an empty `params_json` falls back to `1.0 / 0.07 / 15.0` instead of `2.0 / 0.1 / 6.0`. *Mitigation*: defaults are non-normative from a hardware perspective; the show still respects any stored parameter values, and the new defaults are visually richer.
- **End-fade means the strip is darkest at the moments when the source reaches an end** — for ~5–10 % of each half-period the source amplitude is near zero. *Mitigation*: this is intentional — the source has zero velocity there, so a low-amplitude source reads more honestly than a bright stationary dot. The wavefronts still cover the strip during this time.
- **Per-wavelength decay means very long strips (300+ pixels) stay well-lit far from the source** — at `decay_rate = 1.0`, brightness at 30 wavelengths away is `exp(-30) ≈ 0`, so it doesn't matter in practice; the *useful* decay length is ~5–10 wavelengths which is 75–150 pixels at default wavelength.

## Open Questions

None blocking. One observation worth tracking:

- The end-fade is linear in source position; a smoother ease-in-out (e.g. cosine-shaped) would feel slightly more natural but adds math and isn't visually distinguishable in motion. Left for a future change if anyone notices.