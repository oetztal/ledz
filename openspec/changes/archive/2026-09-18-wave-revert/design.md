## Context

`wave-polish` (archived 2026-09-18) changed the Wave show's decay formula to per-wavelength, added an end-fade brightness factor, and changed defaults to `1.0 / 0.07 / 15.0`. The intent was to remove an end hot-spot and make the show "more convincing". After watching the result on real strips (60 and 144 pixels), the change is a regression:

| Metric (60-LED strip, source near peak) | wave-interference | wave-polish | Δ |
|---|---|---|---|
| Peak brightness | 0.544 | 0.302 | −44 % |
| Mean brightness across strip | 0.162 | 0.094 | −42 % |
| Half-bright region radius | ~21 px | ~10 px | ~half the size |

Three compounding mistakes:

1. **Per-wavelength decay with `wavelength=15` gives a much narrower bright region** than per-strip-length decay with `decay=2.0` on a 60-LED strip. The "strip-length independence" was technically true but in practice made the bright spot half the size.
2. **End-fade dims the source 60 % of the time.** Whenever the source is anywhere except the dead centre, `end_fade < 1`, and at `source_pos=47` on a 60-LED strip it's `0.41`. This was supposed to fix a hot spot but mostly just made the show dimmer.
3. **Slower frequency and longer wavelength** mean less visual activity per second and fewer wavefronts visible at once — sparser and calmer, not richer.

The wave-interference implementation had real issues (the per-strip-length decay couples to strip length, the cosine source does momentarily stop at the ends), but the polish over-corrected. This change undoes it. The end-hot-spot mitigation and per-wavelength decay are captured in the archived `wave-polish/design.md` as "Future possibilities" if anyone wants to revisit them later with better tuning.

### Constraints

- No PSRAM, ~200 KB available RAM. No per-frame allocations (already true, will stay true).
- 100 Hz LED task, 10 ms budget per frame on Core 1.
- NVS compatibility: stored configs that omit fields continue to load via ArduinoJson's `|` operator. Only the fall-back defaults change.

## Goals / Non-Goals

**Goals:**

- Restore the wave-interference code, docs, and spec exactly. No new behaviour.
- Confirm via the native test suite that the reverted implementation still passes all 123 tests.

**Non-Goals:**

- Re-attempting the polish with better parameters. The end-fade and per-wavelength decay are tabled; if revisited, they should be a fresh change with their own measurement-driven defaults.
- Changing other shows, the JSON contract, the parameter storage, the web UI, or anything else.

## Decisions

### Decision 1: Restore per-strip-length decay

```
envelope(i, t) = exp(-decay_rate * |i - source_pos(t)| / N)
```

Same formula as `wave-interference`. `decay_rate=2.0` on a 60-LED strip gives half-brightness at `N * ln 2 / 2 ≈ 21` pixels from the source — a wide bright region that fills most of the strip.

**Alternatives considered:**

- *Per-wavelength decay kept, default tuned*: `decay_rate=0.3` with `wavelength=15` would give similar falloff to the old per-strip-length default on a 60-pixel strip, but only on that strip length. On 30-LED and 144-LED strips the visual would still differ. And the end-fade would still cut brightness. Rejected: this is just `wave-polish` with extra tuning, and the polish was the wrong direction.
- *Per-wavelength decay kept, end-fade removed*: same coupling issue. Rejected.

### Decision 2: Restore defaults `2.0 / 0.1 / 6.0`

| Parameter | wave-polish (reverted) | wave-interference (restored) |
|---|---|---|
| `decay_rate` | 1.0 | 2.0 |
| `brightness_frequency` | 0.07 | 0.1 |
| `wavelength` | 15.0 | 6.0 |

The wave-interference defaults produced a visually richer result on the same hardware. No reason to deviate.

### Decision 3: Remove end-fade entirely

The end-fade was a one-line `end_fade = 1 - 2 * |source_pos / (N - 1) - 0.5|` factor multiplying `source_brightness`. Removing it is one less thing to compute and one less thing wrong with the default behaviour. The cosine source's brief stop at each end is mild and does not need correction.

### Decision 4: No new tests

The test fix made during `wave-polish` (`test_wave_symmetric_lighting_around_mid_source` now calls `execute` ten times to advance `time` to 0.5 s) is preserved — it more accurately tests what the test's own comment claims it tests, and the reverted math still passes it.

## Risks / Trade-offs

- **End hot-spot returns.** Cosine source has zero velocity at the extremes, so for a few frames per bounce the source sits at one end. With the reverted `decay_rate=2.0` and `wavelength=6.0` the bright region is wide enough that this is not visually distracting on a 60+ pixel strip. On very short strips (<20 pixels) the bright source at the end may look like a "stuck" pixel; users with such strips can lower `decay_rate` themselves.
- **Existing NVS configs will fall back to the old defaults.** Anyone who relied on the wave-polish defaults (e.g. through a previous boot) gets the wave-interference behaviour on next reboot. This is the desired outcome.
- **Per-strip-length decay couples the visual to strip length.** Same trade-off the wave-interference implementation already accepted; not solved by this revert.

## Open Questions

None. The "strip-length-independent decay" question from `wave-interference/design.md` remains open and is now captured in `wave-polish/design.md`'s rejected alternatives as a reminder that the simpler answer is to leave the original semantics alone.