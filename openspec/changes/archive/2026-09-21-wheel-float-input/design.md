## Context

`src/color.cpp` defines the shared `wheel(unsigned char)` hue-to-RGB utility. Six shows use it: Rainbow, Mandelbrot, Chaos, MorseCode, TheaterChase, Wave. The function walks the six faces of the RGB cube at full saturation/value via a 6-branch integer table; the body was reworked in `wave-revert-wavelength-and-fix-wheel` from Adafruit's edge-walking version to the HSV cube-walking version that reaches pure yellow/cyan/magenta.

Three of the six callers already operate in float and then truncate before calling:

- `Rainbow.cpp` does `static_cast<uint8_t>(fmodf(hue_position, 255.0f))` on a hue that may already be a continuous float product.
- `Wave.cpp` does `int(emission_time * 20.0f) % 255` plus explicit negative-shift before casting.
- `TheaterChase.cpp` does `static_cast<uint8_t>(cycle_position * 255.0f)` on a fractional cycle position.

The truncation is a real loss: slow animations visibly snap to 1/255 hue steps per frame, and Rainbow with small `pixel_step` collapses adjacent pixels to identical colours. The hardware (ESP32-S3) has a single-precision FPU; float arithmetic in the per-pixel loop is already routine (`cosf`/`sinf`/`expf` in Wave, `fmodf` in Rainbow), so widening `wheel()` does not introduce a new cost class.

## Goals / Non-Goals

**Goals:**
- Recover the fractional hue precision that three callers already compute but discard.
- Produce byte-identical output to the current `wheel(unsigned char)` for every integer input in `[0, 254]`, so the three integer callers (Chaos, Mandelbrot, MorseCode) and every existing persisted NVS parameter set observe no visible change.
- Pin the wrap, clamp, and NaN-handling policy via spec and tests so future changes have a contract to modify.

**Non-Goals:**
- Adding new parameters to any show or web UI control.
- Changing the rainbow-show or wave-show specs (their existing scenario texts remain true under the new signature).
- Touching the integer callers Chaos, Mandelbrot, MorseCode (output is byte-identical; nothing to gain).
- Replacing the existing six-branch cube-walk with a true HSV-to-RGB float formula. The branch-table generalisation is cheaper and reaches the same colours.
- Defining a "no-wrap" or "no-clamp" call mode. Wrap-then-clamp-at-254 is the only behaviour.
- Adding a `wheel_smooth` companion function. The single function is enough.

## Decisions

### Decision 1: Single function with float signature, not a parallel `wheel_f`

**Rationale:** All six callers can move to the float version (three with simplification, three unchanged because they pass integers). Two parallel functions double the maintenance surface for no benefit. The function's contract becomes "hue index is a real number"; integer hues are a natural subset and produce identical output.

**Alternatives considered:**
- `wheel(uint8_t)` + `wheel_f(float)` overloads: forces each caller to pick, and the integer path becomes dead weight once all three float callers migrate. Rejected.
- `wheel(uint8_t)` + `wheel_smooth(float)` named companions: same maintenance issue, plus the names invite confusion ("smooth" suggests temporal dithering, not just float-precision). Rejected.

### Decision 2: Wrap, not clamp

**Rationale:** Three callers already do `fmodf`-style wrap on the float hue before passing it; with wrap inside `wheel()` they can drop their own wrap. The cost is one `fmodf` per call (~10 FPU cycles). Negative inputs from `Wave::execute()` (when a pixel is ahead of the source and `emission_time * 20` is negative) wrap cleanly to a positive hue, replacing the current `if (raw < 0) raw += 255` dance.

**Clamp** (the prior behaviour) was only useful for inputs that were the result of `uint8_t` arithmetic — which can no longer happen. Wrap is strictly more useful.

**Alternatives considered:**
- Clamp (status quo): would force callers to keep their own `fmodf` and to keep the negative-shift in Wave. Rejected — adds code without adding capability.
- Throw on out-of-range: would force every caller into try/catch on the per-pixel loop. Rejected — performance and code-cleanliness both worse.

### Decision 3: NaN → black, ±Inf → black

**Rationale:** A non-finite hue is a caller bug (zero is an upstream mistake if it produces NaN/Inf). The two reasonable fail-safes are "produce black" or "produce whatever the bit pattern happens to mean" (undefined behaviour). Producing black is visibly wrong but not destructive and easy to spot in the field; producing garbage is silent corruption. Black wins.

**Alternatives considered:**
- Assert and crash: traps the bug early in dev but bricks the device in the field. Rejected.
- Return first colour (red) as a sentinel: doesn't distinguish a sane red from a NaN-induced red. Rejected.

### Decision 4: Cube-walk with 6 float sectors, not a closed-form HSV-to-RGB

**Rationale:** The natural float generalization of the existing integer branch table is `pos = h * 6 / 255`, `section = (int)pos`, `frac = pos - section`, then 6 `if`s picking the same channel-ramp the integer body uses. Algebra shows this is byte-identical at every integer input — the proof is `255 * X / 255 = X` for integer `X < 2²⁴`. The integer body had 6 branches; the float body has the same 6 branches plus a multiply, divide, and subtract (≈ 3-4 FPU cycles extra).

**Alternatives considered:**
- True HSV-to-RGB via `c`, `x`, `m` formula: more elegant but adds a `fabsf` and a `% 2.0` per call (~6-8 extra FPU cycles) for no behavioral benefit. Rejected for cost on a per-pixel-per-iteration loop.
- LUT-driven float wheel: would need 256-entry float array; ~1KB of flash for no perceptible quality gain. Rejected.

### Decision 5: Three callers simplify; three stay unchanged

**Rationale:** Rainbow/Wave/TheaterChase already truncate a float hue — moving them to direct float pass *removes* code (no behavior change, just less). Chaos/Mandelbrot/MorseCode already pass integer hues — leaving them alone preserves byte-identical output without risking a different `fmodf` rounding.

The change to the three callers is a strict simplification: fewer lines, identical output at integer inputs (and at every integer value their previous code would have generated), smoother output at fractional inputs.

### Decision 6: Tests pin the byte-identical-at-integers guarantee

**Rationale:** The integer-callers' "no behavior change" claim must be machine-checked, not hand-waved. A loop test that asserts `wheel((float)h) == wheel((uint8_t)h)` for every `h ∈ [0, 254]` is short, fast, and gives a definitive answer. Wrap, NaN, and mid-sector pure-colour tests lock the rest of the new contract.

**Alternatives considered:**
- Property-based fuzzing (e.g. catch): overkill for a 255-pos test table. Rejected.
- Visual diff via the simulator: useful for end-to-end confidence but doesn't pin the function's local contract. Rejected as a substitute; could be added in a future change.

## Risks / Trade-offs

- **[Per-call CPU cost grows ~3-4 FPU cycles]** → At 144 LEDs × 100 Hz × 1 wheel call per pixel, that's ~50 k extra FPU cycles per second on a 240 MHz CPU = 0.02% CPU. Negligible. *No mitigation needed.*
- **[Wrap policy changes for callers that previously clamped]** → The only integer-clamped caller was the `wheel()` body itself, and it just clamped its *own* `uint8_t` input. No external caller relied on the clamp. *Mitigation: documented in spec; tested.*
- **[NaN → black is silent failure]** → A caller bug that produces NaN hues will manifest as "all pixels off" rather than a visible glitch. *Mitigation: spec scenario names "non-finite input returns black" explicitly; existing callers never produce NaN since their inputs are bounded floats from `cosf`/`sinf`/`expf` results.*
- **[Smoother animations may surprise users who expected 1/255 steps]** → Rainbow with default `time_step=1.0, pixel_step=1.0` is byte-identical (both are integers), so the surprise only hits users on non-integer Rainbow configs, which they configured themselves. *Mitigation: explicitly called out in the proposal's Impact section; no code-level fix needed.*
- **[Wave show's `wheel` per-pixel call shape changes]** → The Wave spec's "one wheel per pixel per iteration" requirement still holds; the call is now `wheel(emission_time * 20.0f)` instead of `wheel(uint8_t(int(emission_time * 20.0f) % 255))`. *Mitigation: same call count, no spec change.*
- **[The branch table no longer matches `scripts/wave_show.py`'s float-HSV formula at non-integer inputs]** → `wave_show.py` uses `c, x, m` HSV-to-RGB at float hue; the C++ float wheel uses 6 cube-walk sectors. Algebra shows the two agree at every integer input (within ±1, as already documented for the integer version). At fractional inputs they may differ by 1-2 channel units depending on rounding mode. *Mitigation: not in scope for this change; the reference script is a preview tool, not a contract.*

## Migration Plan

This is a single-step change: edit `src/color.h`/`src/color.cpp`, edit three caller files, add tests. No phased rollout, no feature flag.

- **Apply order:** `color.cpp` body → `color.h` signature → three caller files → `test_color.cpp` additions. The body must change before the signature to avoid a temporary window where the function is float-input but still uses the integer branch table (which would compute wrong section indices for `h > 254`). In practice the whole change is one commit.
- **Rollback:** `git revert` the commit. The integer branch table is preserved in git history.
- **Verification:** `pio test -e native` (must show 126+8 = 134 tests passing); visual diff of Rainbow/Wave/TheaterChase at fractional hue configs if anyone is feeling thorough.

## Open Questions

None. All design choices (signature, wrap, NaN, branch table, three-caller simplification, test scope) are settled. The proposal's user-confirmed decisions (wrap, NaN→black, add tests) are encoded directly in the design above.
