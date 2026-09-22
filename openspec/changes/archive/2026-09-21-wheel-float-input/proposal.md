## Why

The shared `wheel()` color utility in `src/color.cpp` currently takes an `unsigned char` hue index in `[0, 254]`, then integer-arithmetic walks the six faces of the RGB cube to produce a `Strip::Color`. Three shows already compute hue indices as floats and then truncate to integer before calling `wheel()`:

- `Rainbow::execute()` does `static_cast<uint8_t>(fmodf(hue_position, 255.0f))` to wrap its float `hue_position` into a uint8
- `Wave::execute()` does `int(emission_time * 20.0f) % 255` (with explicit negative-handling) before the cast
- `TheaterChase::execute()` does `static_cast<uint8_t>(cycle_position * 255.0f)` to chop its fractional cycle position

This rounding is a real loss: slow animations visibly snap to 1/255 hue steps per frame, and pixel-by-pixel gradients with small `pixel_step` collapse adjacent pixels to identical colors. Three other shows — Chaos, Mandelbrot, MorseCode — pass integer hue indices and are unaffected by either path.

Making `wheel()` accept `float` recovers that fractional precision in the three callers that already throw it away. Algebra shows the new function is byte-identical to the existing one at every integer input in `[0, 254]`, so no show's rendered output changes for any existing user — the smoothness only kicks in where the caller would otherwise have truncated.

## What Changes

- **BREAKING (signature only)**: `wheel()` changes from `unsigned char` to `float`. Existing call sites compile unchanged because uint8 → float is implicit promotion; the returned `Strip::Color` is byte-identical at integer inputs.
- `wheel(float h)` semantics:
  - `NaN` (and `±Inf`) input returns `0x000000` (black) — caller's bug, fail safe.
  - All other finite inputs wrap continuously via `fmodf(h, 255.0f)` and a single `< 0` shift, landing in `[0, 255)`. Out-of-`[0, 254]` inputs that fall inside `[254, 255)` are clamped to 254 so the float-computed hue resolves to the same `wheel(254)` color the integer version produced.
  - The body is the natural float generalization of the existing 6-sector branch table: `pos = h * 6 / 255`, `section = (int)pos`, `frac = pos - section`. The sector widths are now uniformly 42.5 wide in float space (vs. 43/42/42/42/42/43 in integer space). At every integer input the computed channels round to the exact same uint8 values the integer branches produce.
- `Rainbow::execute()`, `Wave::execute()`, and `TheaterChase::execute()` drop their per-call casts and modular arithmetic and pass the float hue directly. Net source-line reduction across the three files: ~5 lines.
- Eight new test cases in `test/test_color/test_color.cpp` pin the contract: byte-equivalence at every integer input in `[0, 254]`, wrap at 255 and 510, negative-wrap at `-1` and `-256`, NaN/Inf → black, and pure-yellow / pure-cyan / pure-magenta at the three mid-sector hues (which the integer version cannot reach).

## Capabilities

### New Capabilities
- `color-wheel`: The shared hue-to-RGB utility used by Rainbow, Mandelbrot, Chaos, MorseCode, TheaterChase, and Wave. The change formalizes its input contract (accepts `float` hue indices, wraps continuously, returns black for non-finite input, and is byte-identical to the prior `unsigned char` behavior at every integer input in `[0, 254]`).

### Modified Capabilities
None. The per-show specs reference `wheel()` only by name (e.g. "hue SHALL drift continuously as wavefronts age" in `wave-show`, "visually equivalent to the prior fixed `wheel((iteration + index) % 255)` behavior" in `rainbow-show` for the default-parameter case). Float-vs-integer input is an implementation detail and the byte-identical-at-integer-inputs guarantee means the existing scenario texts remain true without rewording.

## Impact

- **Code**: `src/color.h` (signature), `src/color.cpp` (body), `src/show/Rainbow.cpp` (drop fmodf+cast), `src/show/Wave.cpp` (drop int-cast + mod + negative-shift), `src/show/TheaterChase.cpp` (drop cast).
- **Tests**: `test/test_color/test_color.cpp` adds eight test cases listed in `tasks.md`. The existing six non-wheel tests in that file are untouched.
- **Per-show callers**: Chaos, Mandelbrot, MorseCode are unchanged — they already pass integer hues and output is byte-identical.
- **Wave spec**: the existing requirement "at most one `exp`, one `fabs`, and one `wheel` call per pixel per iteration" continues to hold — `wheel()` is still called once per pixel.
- **NVS**: no migration. No persisted format changes.
- **Web UI**: no changes. No new parameters exposed.
- **Existing users**: no visible change for any default-parameter show (byte-identical at integer inputs). Smoother animations and gradients for users who already configured `Rainbow` with non-integer `time_step` / `pixel_step`, `Wave` is unchanged visually because it always emits at integer hues, `TheaterChase` cycle colors flow continuously instead of stepping.
- **Cost**: ~6-8 extra FPU cycles per `wheel()` call vs. the integer version. On ESP32-S3 hardware FPU at 240 MHz, with `wheel()` called once per pixel at 100 Hz on up to 144 LEDs, that's ~0.05% CPU. Negligible.
