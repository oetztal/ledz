## Context

The Wave show has a single mode — cosine-bouncing source — that the `wave-revert` change restored earlier today. That implementation is stable and tested. This change extends Wave to a second mode without disturbing the default.

The interesting design question is what "more interesting" means at the math level. The current code computes

```
phase(i, t) = 2π · (i − source_pos(t)) / wavelength
```

which is purely a function of space relative to the current source position. The bright stripes therefore phase-lock to the source: when the source moves, the stripes move with it. What you see on the strip is a *comet bouncing back and forth*, not a wave traveling outward.

A genuine traveling wave would add a time-dependent phase term so stripes drift independently of source motion, at a phase velocity of `brightness_frequency × wavelength` pixels per second. Inside the bouncing envelope, the stripes slide — like wind through a window.

Two side conditions also need addressing:

1. `data/control.html` already has a `waveSpeed` input whose value is sent as `wave_speed`, but `ShowFactory` has silently ignored `wave_speed` since `wave-interference`. The UI should not pretend the parameter exists.
2. The current `wave-show` spec includes a "no new web UI parameters" requirement that this change violates by construction. It must be removed.

## Goals / Non-Goals

**Goals:**

- Add a `mode` parameter with values `"bounce"` and `"traveling"`. Default `"bounce"` so existing configs and call sites are unaffected.
- Implement `traveling` mode so stripes drift across the strip at `freq × wavelength` pixels per second, while the bouncing brightness envelope still applies.
- Remove the dead `waveSpeed` UI input; add a `waveMode` dropdown.
- Reuse the existing hue-from-emission-time computation in both modes (no new color logic).
- Keep the per-pixel cost identical to today's spec: `1 sin + 1 exp + 1 fabs + 1 wheel`.
- Keep the spec's "no per-frame allocation" guarantee.

**Non-Goals:**

- Pulse / dual-source / standing-wave modes. Listed as v2 possibilities; out of scope here.
- A separate per-direction parameter. Negative `brightness_frequency` reverses drift direction (already permitted by the API contract for `Rainbow`; Wave adopts the same convention).
- Changing the existing three parameters' defaults. Decay / wavelength / frequency semantics stay the same in `bounce`; `frequency` is reinterpreted in `traveling` (documented in spec).
- Changes to other shows, the JSON contract, NVS storage, OTA, or the web UI's other shows.

## Decisions

### Decision 1: Single Wave class with `enum class WaveMode`

```cpp
enum class WaveMode { Bounce, Traveling };

class Wave : public Show {
public:
    Wave(float decay_rate = 2.0f,
         float brightness_frequency = 0.1f,
         float wavelength = 6.0f,
         WaveMode mode = WaveMode::Bounce);
    ...
};
```

`mode` goes last with a default so existing direct constructor calls (`Wave()`, `Wave(3.5f, 0.5f, 10.0f)`) continue to compile. The factory is the only code that constructs a `Wave` from JSON, and the factory supplies `mode` explicitly from the parsed JSON (or the default when absent).

**Alternatives considered:**

- *Separate `WaveBounce` / `WaveTraveling` classes with a shared base*. Cleaner per-mode specs, but the math is 95 % identical (same envelope, same hue, same per-frame loop shape) and the duplicated scaffolding is not worth the file count.
- *String constants instead of enum*. Less type safety; no compile-time protection against typos like `"travaling"`. Rejected.

### Decision 2: JSON `mode` is a string with fall-back

```cpp
const char *mode_str = doc["mode"] | "bounce";
WaveMode mode = (strcmp(mode_str, "traveling") == 0)
              ? WaveMode::Traveling
              : WaveMode::Bounce;
```

Unknown values fall back to `bounce` (same fallback semantics ArduinoJson already provides for missing fields). No error is raised; the spec documents this.

**Alternatives considered:**

- *Reject unknown values with HTTP 400*. Strict, but breaks backward compatibility for any caller that has hand-crafted an unsupported value, and silently-ignoring is consistent with how `wave_speed` is handled today.

### Decision 3: Traveling phase is current phase minus `2π · t · freq`

```cpp
// Both modes share source_pos(t), envelope, source_brightness, hue.
float phase = (mode == WaveMode::Traveling)
    ? 2.0f * M_PI * (distance * inv_wavelength - time * brightness_frequency)
    : 2.0f * M_PI *  distance * inv_wavelength;
float wave_brightness = fabsf(sinf(phase));
```

The traveling phase has a single new term `−2π · t · brightness_frequency` and reuses every other variable from the bounce path. The per-frame loop body is unchanged in shape; one branch decides which phase formula to use.

**Why not just add the time term without the spatial `i/λ` factor?** The full expression `2π · (i/λ − t·freq)` is the canonical traveling-wave phase, where `i/λ` is the spatial wave number times position and `t·freq` is the temporal phase advance. Stripes then drift at `freq · λ` pixels per second, which is what we want.

**Why not also subtract `2π · source_pos(t) / λ`?** We don't need to. In the current code the phase depends only on `(i − source_pos)` because the source carries the pattern. In traveling mode, adding `−2π · t · freq` is enough to detach the stripes from the source motion. A source-phase offset would shift the pattern's hue but not change the geometry. (Verified by writing the phase out and reading the difference of two adjacent pixels: `(i+1)/λ − i/λ = 1/λ`, independent of source position.)

**Alternatives considered:**

- *Use a different `freq` parameter for traveling and rename the existing one*. Cleaner semantics ("freq" is ambiguous across modes) but adds a new UI knob. Rejected for v1.
- *Compute traveling via two `sinf` calls and average*. Mathematically equivalent to one phase computation, but slower. Rejected.

### Decision 4: Hue computation is shared between modes

The hue-from-emission-time formula

```cpp
float propagation_speed = N * brightness_frequency;
float emission_time = color_time - abs_distance / propagation_speed;
uint8_t color_index = (uint8_t) ((int) (emission_time * 20.0f) % 255);
Strip::Color pixel_color = wheel(color_index);
```

is reused in both modes. In `bounce` it produces a "new hue near source, old hue at ends" gradient that follows the bouncing source. In `traveling` it produces a slightly different effect (since the wavefronts are now drifting) but it is consistent and the resulting look is coherent: the rainbow trail still feels tied to wavefront age rather than to absolute position.

**Alternatives considered:**

- *Hue from local phase `i/λ − t·freq`*. Cleaner conceptually ("hue travels with the wave") but adds a new formula to spec and the difference is not visually compelling enough to justify the spec surface.

### Decision 5: UI replaces `waveSpeed` with `waveMode` dropdown

```
┌─ Wave parameters ──────────────────────────────────────┐
│  Mode            [Bounce ▼] │  ← new
│  Decay Rate      [ 2.0  ]   │
│  Bounce Freq     [ 0.10 ]   │  ← label changes per mode
│  Wavelength      [ 6.0  ]   │
│                  [ Apply Parameters ]                  │
└─────────────────────────────────────────────────────────┘
```

`applyWaveParams()` sends `{mode, decay_rate, brightness_frequency, wavelength}` — no more `wave_speed`. `populateShowParams()` reads `params.mode` and ignores any `params.wave_speed` it sees (preserving silent-compat with old NVS configs that still contain it).

The frequency field's help text changes between modes: *"Bounces per second"* in bounce, *"Phase cycles per second — drift = freq × wavelength px/s"* in traveling. One `<small>` element, text toggled by JavaScript when the dropdown changes.

**Alternatives considered:**

- *Keep `waveSpeed` for one release and mark it deprecated*. Avoids a UI change in the same PR as a backend change but is dishonest about what the input does. Rejected.
- *Move mode to the bottom of the panel*. Less prominent but consistent with how new parameters are usually added in this codebase. I prefer it first because mode is the primary choice; the rest are tuning.

### Decision 6: One spec capability with mode-conditional requirements

Keep `wave-show` as a single capability. Existing bounce-specific requirements become `WHEN mode == "bounce"` conditions. New requirements for traveling are added under `## ADDED Requirements`. The "no new web UI parameters" requirement is removed (this change is the explicit replacement).

**Alternatives considered:**

- *Split into `wave-bounce` and `wave-traveling` capabilities*. Cleaner per-mode specs, but breaks the single dropdown entry that users see and complicates persistence (NVS stores one `params_json` per show name). The 95 % overlap between modes makes a single capability more cohesive. Rejected.

## Risks / Trade-offs

- **End hot-spot still exists in traveling mode.** Same as bounce — the cosine source has zero velocity at the extremes. The previous `wave-polish` change tried to mitigate this with an end-fade factor and was reverted because it made things dimmer. Same mitigation is not part of this change. Acceptable. → None.

- **Traveling with `freq=0.1` and `λ=6` drifts at 0.6 px/s on a 60-LED strip.** That's one wavelength per bounce cycle — slow. Users who want fast drift will need to bump `freq` or shorten `wavelength`. The UI help text guides them. → Documented in spec + UI help text.

- **Unknown `mode` values silently fall back to bounce.** A typo like `"travaling"` looks identical to omitting the field. → Documented in spec; not worth a 400 error in v1.

- **The dead `waveSpeed` UI input is being deleted, not deprecated.** Any external HTML scraping of the old input element will break. → Acceptable; the input was a no-op anyway.

- **Per-pixel phase computation gains two multiplies in traveling mode.** Trivial cost (one ARM instruction each). The spec's "one `sin`, one `exp`, one `fabs`, one `wheel` per pixel" requirement still holds. → None.

## Migration Plan

No migration is needed. Deployment proceeds as a normal `pio run -t upload`:

1. New firmware ships with `mode` parameter and `traveling` implementation.
2. Existing NVS configs without `mode` field load with `mode="bounce"`. Behavior unchanged.
3. Web UI for Wave now exposes the mode dropdown. Users who never touch it see no change.

If `traveling` proves broken on hardware, rollback is a single OTA revert to the previous firmware release; `bounce` is unchanged and the new `mode` parameter defaults to it.

## Open Questions

None. The four confirmed decisions (single class, JSON string with fallback, additive time phase term, shared hue formula) plus the UI change cover the design. Any future mode additions (`pulse`, `dual`, `standing`) will require a separate change.