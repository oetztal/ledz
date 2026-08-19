## Context

The Rainbow show (`src/show/Rainbow.cpp`) is a 15-line animation that calls `wheel((iteration + index) % 255)` for every pixel. It is the default show on first boot (`ShowController.cpp:47`) and has the simplest possible animation math in the codebase.

Parameter configuration is already a well-established pattern in this project. Six shows (Solid/ColorRanges, Mandelbrot, Chaos, Fire, Starlight, Wave, MorseCode, TheaterChase, Stroboscope) already accept JSON params via `ShowFactory`, and the web UI (`data/control.html`) follows a consistent pattern: a `params-section` div with number inputs, an `applyXxxParams()` function, a case in `updateParameterVisibility()`, and a case in `populateShowParams()`. Adding Rainbow to this pattern is largely a copy-paste exercise with two slider inputs instead of number inputs.

The one UI deviation here is the use of `<input type="range">` sliders. Sliders are not currently used for any show parameter (only brightness uses one). This change introduces that UI element for show parameters for the first time.

## Goals / Non-Goals

**Goals:**
- Make Rainbow's two hue-step multipliers user-configurable through the web UI and `/api/show` endpoint
- Preserve existing behavior when no params are provided (or `{}` is sent)
- Match the established show-parameter UI pattern as closely as possible
- Add basic native test coverage for the Rainbow math (currently zero)

**Non-Goals:**
- Adding touch-controller variants for Rainbow
- Validating or rejecting out-of-range parameter values server-side
- Refactoring other shows to use sliders
- Splitting float iteration into integer + fractional counters for long-run precision (premature)
- Adding color presets or named Rainbow variants in the UI

## Decisions

### 1. Float type for both parameters

**Decision:** Both `time_step` and `pixel_step` are `float`.

**Why:** User specified floats. Floats allow finer-grained control than integers (e.g., `0.3` for a slow drift), and the cost in memory or performance is negligible — two extra floats per show instance.

**Alternative considered:** `unsigned int`. Cheaper, but rejects fractional values and was explicitly not preferred.

### 2. UI sliders, not number inputs

**Decision:** Use `<input type="range">` with a live value display, range `0.0–5.0`, step `0.05`.

**Why:** User explicitly requested sliders. Continuous visual control feels right for two parameters that smoothly map to animation speed and density.

**Range rationale:**
- `time_step=1.0` is the classic rainbow (~2.5s per hue cycle at 100Hz). `5.0` gives ~0.5s per cycle — fast and visually distinct. Going higher blurs into indistinguishability for most strips.
- `pixel_step=1.0` fills the strip with one full rainbow. `5.0` compresses five full rainbows into the strip — visually still distinct. Going higher starts to look like solid color blocks.
- `step=0.05` gives 100 distinct slider positions in the 0–5 span, enough resolution for fine-tuning.

**Alternative considered:** Number inputs. Simpler, more consistent with existing shows, but less interactive. Rejected per user preference.

### 3. No client-side validation of negative values

**Decision:** HTML inputs enforce `min="0"`, but the C++ side does not reject negative values.

**Why:** User said "no negatives *for now*", which signals the policy could change. If we add validation later, a separate decision can be made about whether to clamp or reject. Today, sending `{"time_step":-1}` via the API works and reverses the scroll direction — a documented feature in spirit, even if not exposed in the UI.

**Alternative considered:** Hard-reject negatives in the factory. Tighter contract, but premature given the user's "for now" qualifier.

### 4. Math: float multiplication then `fmodf`

**Decision:**
```cpp
float hue_position = static_cast<float>(iteration) * time_step
                   + static_cast<float>(index) * pixel_step;
uint8_t hue_index = static_cast<uint8_t>(fmodf(hue_position, 255.0f));
strip.setPixelColor(index, wheel(hue_index));
```

**Why:** The simplest formulation that supports fractional step sizes. `fmodf` handles the wraparound and the existing `wheel()` safeguard (clamps to 254) remains a defense-in-depth.

**Precision concern:** `float` has ~7 significant decimal digits. At iteration `1.7×10¹⁰` (~6 months at 100Hz), precision loss becomes visible. Acceptable: the visual difference is sub-frame and indistinguishable from natural float drift. If this becomes a real problem, splitting into integer+remainder counters is a separate change.

**Alternative considered:** Cast iteration to `double` for the multiplication. Marginal precision improvement, ~2× in the worst case, but doubles the arithmetic cost and still has a horizon. Not worth the trade.

### 5. Defaults preserve current behavior exactly

**Decision:** Both parameters default to `1.0f`.

**Why:** With both defaults, the new math reduces to `(iteration + index) % 255` rounded through float — visually identical to the current `(iteration + index) % 255` integer path at every reachable iteration count for any reasonable strip length. Backward compatibility is a one-liner.

### 6. No test for the wheel function

**Decision:** Tests cover the multiplier formula only (`time_step` and `pixel_step` arithmetic), not the `wheel()` color conversion.

**Why:** `wheel()` is exercised by other shows (Starlight, TheaterChase, etc.) and a dedicated test exists or could exist in `test/test_color`. Testing it again here would be redundant.

## Risks / Trade-offs

- **Slider UI precedent** → First use of `<input type="range">` in show params. Other shows may want similar treatment later; CSS may need tuning. Mitigation: keep the styling minimal and copy-pasteable.
- **Float precision at extreme uptime** → Hue drift becomes visible after ~6 months. Mitigation: documented; the visual effect is sub-frame and unnoticeable in practice. If reported, a follow-up change can split counters.
- **Web UI input min mismatch with API** → HTML rejects negative values, API accepts them. Mild inconsistency. Mitigation: documented decision; either input channel behaves as expected for its audience.
- **`params-section` reflow** → Adding a new section shifts the existing show selector UI downward. Mitigation: identical to every other params section added in this codebase.
- **Touch button still cycles into Rainbow with defaults** → Pressing the show-cycle button repeatedly will land on Rainbow with `time_step=1.0, pixel_step=1.0`. The user may want different defaults via touch cycling. Mitigation: user opted out of touch variants for now.

## Migration Plan

No migration needed. Stored `params_json` of `{}` (the value every Rainbow device has today) parses to `time_step=1.0, pixel_step=1.0` via the `|` default operator, reconstructing the prior animation exactly. Devices rebooting after the OTA upgrade will look identical until the user opens the web UI and adjusts the sliders.

## Open Questions

None. All design decisions captured above.