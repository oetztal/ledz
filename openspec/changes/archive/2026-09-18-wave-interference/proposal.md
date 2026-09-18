## Why

The current Wave show is monotone: a single sine wave emanates from one end of the strip with exponential decay, producing a short-lived pulse that quickly fades to dark across most of the strip. There is no reflection, no interference, and no sense of the strip as a resonant space — the user perceives the same predictable pattern repeating forever. This change refreshes Wave with a bouncing source so the strip naturally fills with overlapping wave trains and self-interferes, while keeping the existing parameter names and JSON contract intact for current users.

## What Changes

- Rewrite `src/show/Wave.cpp` so the wave source oscillates smoothly between the two ends of the strip, producing reflected waves and natural interference patterns where wavefronts overlap.
- Update `src/show/Wave.h` to document the new parameter semantics. The `wave_speed` parameter is removed (its effect is now implicit in source motion); `decay_rate`, `brightness_frequency`, and `wavelength` keep their names but mean different things internally.
- Update `ShowFactory.cpp` Wave registration: stop parsing `wave_speed` from JSON (silently ignored if present so existing configs don't break), keep the other three parameters with their defaults.
- Update the human-readable Wave description in the show list to reflect the new behavior.
- Add Wave smoke tests to `test/test_shows/` so the refreshed implementation has at least minimal coverage, matching the precedent set by Rainbow tests in the same file.
- Update `docs/SHOW_PARAMETERS.md` Wave section to describe the new parameters and behavior (post-implementation).

No new parameters are exposed to the UI in this change. The wave becomes more interesting without adding knobs.

## Capabilities

### New Capabilities

- `wave-show`: defines the visual behavior, parameter contract, and persistence expectations of the Wave show after this refresh. Replaces the implicit pre-existing behavior with an explicit, testable spec.

### Modified Capabilities

None. The other Wave-related capability is the show parameter system (handled in the existing show-system work), which already supports per-show JSON parameters and persistence — no requirement changes there.

## Impact

- **Code**: `src/show/Wave.h`, `src/show/Wave.cpp`, `src/ShowFactory.cpp`, `test/test_shows/test_shows.cpp`.
- **Docs**: `docs/SHOW_PARAMETERS.md` (Wave section only).
- **API**: `POST /api/show` accepts the same three parameters it always did for Wave. `wave_speed` is silently ignored if sent — this is a **soft breaking** change for any external script that depended on `wave_speed` having an effect. NVS-stored configs with `wave_speed` are unaffected because missing fields fall through `ArduinoJson`'s `|` operator to defaults.
- **Hardware**: no change. Same single `sin` + `exp` + `wheel` per pixel per iteration; computationally identical to today.
- **Existing users**: anyone with Wave configured sees different (richer) behavior on next show change or reboot. Defaults keep the strip visibly active at standard strip lengths.

## Future Possibilities

These are explicitly *not* part of this change but are the natural next moves, captured here so they survive as a menu rather than being re-derived later. Each is one parameter + one block of math away from the Path A design.

### 1. Detune / moiré mode

Today the bounce period and wavelength are tuned to keep the ratio `N * brightness_frequency / wavelength` near an integer, so the system is periodic and you can still read "this is a wave". A new parameter `ratio_offset` (float, e.g. default `0.07`) could deliberately perturb this ratio so the system never quite repeats — patterns drift, evolve, dissolve. The aesthetic shifts from "ripples on a pond" to "oil on water / heat shimmer".

This could ship as a single mode switch (`mode: wave | textured`) or as a continuous slider. It is the cheapest way to address the "still feels patterned" feedback that may arise after Path A.

### 2. Second bouncing source

Sum a second independently-bouncing source with the first. Doubles the math (~2× trig per pixel, still trivially within the 100Hz / no-PSRAM budget), creates true multi-source interference patterns. Qualitative jump over (1) is small but additive — the two compose well.

Worth doing if Path A alone still feels too regular, but not necessary as a first extension.

### 3. Envelope LFO

Today the wave amplitude is a pure sine. An additional slow LFO could modulate the envelope shape (sharp crests vs broad valleys vs gaussian packets) so the wave "breathes" over time — a calm phase, a gust phase, a storm phase, repeating. Decoupled from source motion, this affects *texture* without affecting *direction*.

Useful if the user wants more visual variety without changing position/motion.

### 4. Palette refactor

The current color logic uses `wheel()` based on emission time — every wave is rainbow-painted identically. Alternatives to explore:

- **Single-color waves**: each emitted wave inherits one color that drifts slowly, so you see a *color* traveling down the strip rather than a rainbow smear.
- **Locked palette**: cycle through a fixed 4–5 color palette instead of the full wheel.
- **Hue-from-position**: each end of the strip has its own hue, and waves carry both — color moves *with* the wave.

This is a separate aesthetic axis from the wave geometry; independent change.

### 5. Source-intensity fade near ends

The cosine source motion causes the source to linger at each end of the strip, which creates a hot-spot there. A small `(1 - |normalized_source_pos - 0.5| * 2)` factor on source brightness would even out the intensity profile. Only worth doing if the hot-spot becomes visually distracting in practice.

### 6. Triangle (bouncing) source motion

Path A uses cosine source motion — smooth velocity, no jolt at the bounce, but the source lingers at the ends. A triangle-wave source (constant speed, instantaneous direction reversal at ends) would feel more "zippy" and energetic. Trivial swap of one line in the math; worth exposing as a `source_shape: cosine | triangle` parameter if users ask for the energetic feel.

## Non-goals

This change will *not* introduce:

- New parameters in the web UI.
- Changes to other shows (Rainbow, Fire, etc.).
- Changes to the show parameter storage or persistence mechanism.
- Changes to the show list ordering or default show on boot.