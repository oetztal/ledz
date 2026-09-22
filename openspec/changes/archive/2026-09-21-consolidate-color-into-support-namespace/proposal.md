## Why

The codebase currently has two color-utility files coexisting at different layers:

- `src/color.{h,cpp}` — five functions in the global namespace: `wheel(float)`, `color(r, g, b)`, `red(color)`, `green(color)`, `blue(color)`. Four are 1–4 line packers/extractors; `wheel()` is a 30-line cube-walk.
- `src/support/color.{h,cpp}` — `Support::Color::black_body_color(float)`. A non-trivial color utility in the `Support::Color` namespace.

This is the only place in the codebase where two same-named files coexist at different layers, and the root `color.h` is the only place where a non-trivial utility (`wheel`) sits in the global namespace instead of `Support::*`. Every other non-trivial utility (`SmoothBlend`, `Palette`, `Gamma`, `black_body_color`) lives under `src/support/`. Consolidating `wheel` and the trivial packers/extractors into `Support::Color` matches the established pattern: one home for color utilities, one `color.h` to find them in.

## What Changes

- **BREAKING**: All five functions move from the global namespace to `Support::Color`:
  - `wheel(float)` → `Support::Color::wheel(float)` (same signature, same behaviour)
  - `red(Strip::Color)` → `Support::Color::red(Strip::Color)` (same signature)
  - `green(Strip::Color)` → `Support::Color::green(Strip::Color)` (same signature)
  - `blue(Strip::Color)` → `Support::Color::blue(Strip::Color)` (same signature)
  - `color(uint8_t, uint8_t, uint8_t)` → `Support::Color::from_rgb(uint8_t, uint8_t, uint8_t)` — renamed to avoid the awkward `Support::Color::color(...)` form. The `color` name is dropped because using a function with the same name as its enclosing namespace reads as a constructor call and is misleading.
- `src/color.h` and `src/color.cpp` are deleted.
- `src/support/color.h` and `src/support/color.cpp` are expanded to declare and define all six functions (`wheel`, `from_rgb`, `red`, `green`, `blue`, `black_body_color`).
- All 16 callers across `src/`, `test/`, and `support/` switch their include from `#include "color.h"` to `#include "support/color.h"` and prefix every call site with `Support::Color::`.

## Capabilities

### New Capabilities
None.

### Modified Capabilities
- `color-wheel`: each requirement that names `wheel()` will reference `Support::Color::wheel()` instead. No behavioural changes; the contract pinned in `wheel-float-input` (NaN→black, wrap, clamp, byte-identical at integer inputs, mid-sector pure colours) is preserved. One new scenario is added: "`wheel()` is reachable as `Support::Color::wheel()`" so the namespace is explicit in the spec rather than implied.

## Impact

- **Deleted**: `src/color.h`, `src/color.cpp`.
- **Modified**: `src/support/color.h` (add 5 declarations), `src/support/color.cpp` (add 5 definitions; existing `black_body_color` body unchanged).
- **Modified callers (~16 files)**:
  - `src/ShowController.cpp`
  - `src/show/ColorRanges.cpp`, `src/show/Fire.cpp`, `src/show/Mandelbrot.cpp`, `src/show/MorseCode.cpp`, `src/show/Rainbow.cpp`, `src/show/Starlight.cpp`, `src/show/Stroboscope.cpp`, `src/show/TheaterChase.cpp`, `src/show/Wave.cpp`, `src/show/Chaos.cpp`
  - `src/show/factory/ColorRangesFactory.cpp`
  - `src/strip/Layout.cpp`
  - `src/support/Palette.cpp`, `src/support/SmoothBlend.cpp`
  - `test/test_color/test_color.cpp`, `test/test_palette/test_palette.cpp`, `test/test_color_ranges/test_color_ranges.cpp`, `test/test_show_factory/test_show_factory.cpp`
  - `test/MockStrip.h`
- Each caller: `#include "color.h"` → `#include "support/color.h"`; every `wheel(...)` → `Support::Color::wheel(...)`; every `red(...)` → `Support::Color::red(...)` (same for green/blue); every `color(...)` → `Support::Color::from_rgb(...)`.
- **Test impact**: 134/134 tests must still pass. The `test_wheel_byte_identical_at_integer_inputs` and the other 7 new wheel tests stay byte-identical (the function body is unchanged); only call-site syntax changes.
- **No NVS migration**: no persisted format changes.
- **No web UI / spec changes**: web UI doesn't reference C++ color helpers.
- **No behavior change**: every render is byte-identical before and after, modulo the namespace prefix.
