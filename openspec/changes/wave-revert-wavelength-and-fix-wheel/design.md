## Context

`scripts/wave_show.py` is the canonical Python reference for the Wave show. It is stdlib-only, takes the same conceptual inputs as the C++ Wave constructor, and produces pixel-accurate output for parameter tuning and visual previewing. Two specific aspects of the current C++ implementation have drifted from that reference:

1. **`wavelength` parameter**: the C++ constructor takes `wavelength` and uses it for both a phase term (`2π * distance / wavelength`) and the `|sin(phase)|` brightness modulation. The Python reference explicitly omits both: the docstring on `wave_show.py:23-28` says the wavelength modulation "is intentionally omitted here: with a typical strip width the resulting structure is too short to be useful in the preview." On a typical 60-pixel strip, `wavelength=6` gives ten full cycles — visually it's just high-frequency noise that washes out the bouncing source and the hue trail, which are the interesting parts.

2. **`wheel()` function**: the C++ `wheel()` in `src/color.cpp` is a direct port of Adafruit's NeoPixel strandtest example. It walks the edges of the RGB cube and therefore only ever has one channel at full intensity at any position — green → red → blue → green, with transitions through 0-valued channels. Pure yellow (255, 255, 0), cyan (0, 255, 255), and magenta (255, 0, 255) are unreachable. The Python reference (`scripts/wave_show.py:68-95`) walks the six faces of the cube via the standard HSV-to-RGB formula at full saturation and value, hitting all six primary and secondary colours at their nominal hue angles.

The Wave show is used as the preview subject for `scripts/wave_show.py`, so the two must agree byte-for-byte at the same iteration index and strip width. They currently do not, and that gap is the motivation for this change.

## Goals / Non-Goals

**Goals:**

- Make `src/show/Wave.cpp` match `scripts/wave_show.py` byte-for-byte for the per-iteration pixel values at any given strip width, iteration, and parameter set.
- Make `src/color.cpp`'s `wheel()` match `scripts/wave_show.py`'s `wheel()` so all callers (Rainbow, Mandelbrot, Chaos, MorseCode, TheaterChase, Wave) get the richer six-colour palette.
- Drop the `wavelength` parameter cleanly: constructor signature, factory JSON, web UI input, docs, tests, and spec.
- Keep the Wave show's `mode` parameter (`bounce` / `traveling`) accepted for future expansion, but document and test that both modes render identically today (matching the reference, which accepts the parameter but does not branch on it).

**Non-Goals:**

- Adding new visual behaviour to the Wave show. The reference is the design; this change is purely a realignment.
- Changing the `wheel()` calling convention, its signature, or its input range. It stays `wheel(unsigned char)` with the same `[0, 254]` clamping behaviour; only the body changes.
- Tuning any of the other shows that use `wheel()`. The richer palette is a side effect of the fix and is intentional.
- NVS migration. Missing `wavelength` falls back via ArduinoJson's `|`, and the wheel change is purely visual.

## Decisions

### Decision 1: Drop the `|sin(phase)|` term entirely, not replace it with another modulation

The reference has no phase term at all — the per-pixel brightness is just `source_brightness * envelope`. Adding a different modulation to "replace" wavelength would be a new design choice, not a realignment. The Wave show becomes visually simpler: a bouncing rainbow source whose brightness decays exponentially with distance, and nothing else. That matches the reference exactly.

### Decision 2: Keep `mode` as a no-op rather than removing it

The reference accepts `--mode` but does not branch on it. The C++ constructor already takes `mode` and the factory already parses it. Removing it would be a second breaking change on top of the wavelength removal, and the reference's own docstring flags the parameter for "future expansion". Keeping it as an accepted-but-currently-ignored parameter preserves the JSON contract and lets a future change add real travelling-mode behaviour without another contract break.

### Decision 3: Replace `wheel()` globally rather than adding a parallel function

The reference's docstring says the new wheel "replaces" the Adafruit one. All six shows that use `wheel()` (Rainbow, Mandelbrot, Chaos, MorseCode, TheaterChase, Wave) benefit from the richer palette, and none have any contract or test that asserts the old edge-walking output. A parallel `wheel_hsv()` would leave the old function as dead code and force every caller to pick. Global replacement is cleaner and matches the reference.

### Decision 4: Implement `wheel()` as integer arithmetic, not floats

The reference uses floats because Python's HSV-to-RGB is naturally floating-point and the script runs once for previewing. The C++ `wheel()` is called every iteration of every show that uses it (multiple times per pixel for some), so it must stay cheap. The new wheel is implemented with the same integer arithmetic as the old one: `h = wheel_pos * 360 / 255` is replaced with a branch table over six 42-or-43-wide hue sectors (0–42, 43–84, 85–127, 128–169, 170–211, 212–254). Each sector computes `rp`, `gp`, `bp` as `(255 - pos*6)`, `pos*6`, or `0` based on which two of the three channels are transitioning. The result matches the float reference within ±1 on every position — verified by exhaustive comparison.

**Alternatives considered:**

- *Float-based `wheel()`*: simpler to write, matches the reference character-for-character, but adds per-call float math to every show that uses it. Rejected for cost on a 100 Hz LED task with no PSRAM.
- *256-entry LUT*: zero math per call after init, but 768 bytes of RAM permanently consumed. Rejected as too much overhead for a function called many times per frame.

### Decision 5: `wave_speed` is still silently ignored

The previous change already made `wave_speed` a no-op (it was the wave-interference replacement for an even older parameter). The factory parser never read it, but the test still asserts it is ignored. Keep the test as-is — it documents that legacy configs continue to load.

## Risks / Trade-offs

- **[Visual regression on Rainbow / Mandelbrot / Chaos / MorseCode / TheaterChase]** → These shows used the old edge-walking wheel and will look different after the fix: the hue trail will pass through yellow / cyan / magenta where it used to pass through near-black transitions. Intentional (matches the reference), but worth eyeballing each show on a real strip before shipping. Mitigated by the native test suite: every show still constructs and renders without crashing, and the Wave-specific assertions still pass.
- **[Travelling-mode is silently a no-op]** → A user who reads the docs and sets `mode="traveling"` will see the same thing as `mode="bounce"`. The docs are updated to make this explicit, and a test asserts the two modes produce identical pixels.
- **[Wavelength is silently ignored in stored configs]** → Anyone with `"wavelength":N` in NVS sees no effect from that field. The factory parser drops it, the constructor no longer takes it. Existing tests for `wave_speed` ignore cover the same pattern (legacy field silently ignored), so the contract is consistent.

## Migration Plan

No migration. The `wavelength` field is dropped from the JSON contract; ArduinoJson's `|` operator handles missing fields on existing NVS configs. The `wheel()` change is a visual upgrade on next show change or reboot.

Rollback is a single `git revert` of the implementation commits; the spec delta and the archived change capture the old behaviour for reference.

## Open Questions

None.
