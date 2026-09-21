## 1. Support color module additions

- [x] 1.1 Update `src/support/color.h`: add five declarations inside `namespace Support::Color { ... }` for `Strip::Color wheel(float wheel_pos)`, `Strip::Color from_rgb(Strip::ColorComponent red, Strip::ColorComponent green, Strip::ColorComponent blue)`, `Strip::ColorComponent red(Strip::Color color)`, `Strip::ColorComponent green(Strip::Color color)`, and `Strip::ColorComponent blue(Strip::Color color)`. The existing `black_body_color` declaration stays where it is.
- [x] 1.2 Update `src/support/color.cpp`: add five definitions inside `namespace Support::Color { ... }` for the same five functions, with bodies moved verbatim from `src/color.cpp`. The new functions live alongside the existing `black_body_color` body. Add a `#include <cmath>` at the top if not already present (it's currently included for `black_body_color`'s `std::log`).

## 2. Update src/ callers

- [x] 2.1 `src/ShowController.cpp`: switch `#include "color.h"` to `#include "support/color.h"`; prefix the one `color(...)` call at the black-init line with `Support::Color::`.
- [x] 2.2 `src/strip/Layout.cpp`: switch include; prefix the two `color(0, 0, 0)` calls with `Support::Color::`.
- [x] 2.3 `src/support/Palette.cpp`: switch include; prefix the three `red(...)`, `green(...)`, `blue(...)` extractors and the one `color(...)` call with `Support::Color::`.
- [x] 2.4 `src/support/SmoothBlend.cpp`: switch include; prefix the three `red(...)`, `green(...)`, `blue(...)` extractors and the one `color(...)` call with `Support::Color::`.
- [x] 2.5 `src/show/ColorRanges.cpp`: switch include; prefix the three `red(...)`, `green(...)`, `blue(...)` debug-log calls and the six RGB-interpolation calls with `Support::Color::`.
- [x] 2.6 `src/show/Fire.cpp`: switch `#include "support/color.h"` (already correct) to use the new header; no prefix changes (Fire uses `black_body_color`, which is already in the namespace).
- [x] 2.7 `src/show/Mandelbrot.cpp`: switch include; prefix the implicit `wheel(...)` call with `Support::Color::`.
- [x] 2.8 `src/show/MorseCode.cpp`: switch include; prefix the four `color(0, 0, 0)` / `color(255, 255, 255)` pattern-builder calls and the one `wheel(...)` call with `Support::Color::`.
- [x] 2.9 `src/show/Rainbow.cpp`: switch include; prefix the `wheel(...)` call with `Support::Color::`.
- [x] 2.10 `src/show/Starlight.cpp`: switch include; prefix the three `red(...)`, `green(...)`, `blue(...)` extractors and the two `color(...)` calls with `Support::Color::`.
- [x] 2.11 `src/show/Stroboscope.cpp`: switch include; prefix the two `color(...)` calls with `Support::Color::`.
- [x] 2.12 `src/show/TheaterChase.cpp`: switch include; prefix the `wheel(...)` call and the one `color(0, 0, 0)` call with `Support::Color::`.
- [x] 2.13 `src/show/Wave.cpp`: switch include; prefix the `wheel(...)` call, the three `red(...)`, `green(...)`, `blue(...)` extractors, and the one `color(...)` call with `Support::Color::`.
- [x] 2.14 `src/show/Chaos.cpp`: switch include; prefix the `wheel(...)` call with `Support::Color::`.
- [x] 2.15 `src/show/factory/ColorRangesFactory.cpp`: switch include; prefix any color-helper calls (verify with grep) with `Support::Color::`.

## 3. Update test/ callers

- [x] 3.1 `test/test_color/test_color.cpp`: switch include; update the eight `wheel(...)` calls in the wheel tests to `Support::Color::wheel(...)`; update the `reference_wheel_uint8` helper to call `Support::Color::from_rgb(r, g, b)` for its three return statements (the function packs RGB into a Strip::Color); update the `red(...)`, `green(...)`, `blue(...)` calls in the existing tests to `Support::Color::red(...)` etc.
- [x] 3.2 `test/test_palette/test_palette.cpp`: switch include; prefix any `color(...)` / `red(...)` / `green(...)` / `blue(...)` calls with `Support::Color::`.
- [x] 3.3 `test/test_color_ranges/test_color_ranges.cpp`: switch include; prefix any color-helper calls with `Support::Color::`.
- [x] 3.4 `test/test_show_factory/test_show_factory.cpp`: switch include; prefix any color-helper calls with `Support::Color::`.
- [x] 3.5 `test/MockStrip.h`: switch include (it's a header used by every test suite); no prefix changes if it has no color-helper calls — verify with grep.

## 4. Delete old files

- [x] 4.1 Delete `src/color.h`. Verify no `#include "color.h"` references remain in `src/` or `test/`.
- [x] 4.2 Delete `src/color.cpp`. Verify no references to its declarations remain.

## 5. Verify

- [x] 5.1 `pio test -e native` builds clean and the full native suite is green — same 134 tests as before the refactor, none added, none removed.
- [x] 5.2 No file outside `src/support/` includes `"color.h"` (the only three matches are `src/support/{SmoothBlend,color,Palette}.cpp`, which correctly resolve to the new `src/support/color.h`);).
- [x] 5.3 `grep -r 'using namespace Support::Color' src/ test/` returns zero matches — no consumer is allowed to drop the `Support::Color::` prefix.

## 6. Rename to PascalCase

- [x] 6.1 Rename `src/support/color.h` → `src/support/Color.h` and `src/support/color.cpp` → `src/support/Color.cpp`. The filenames now match the `Support::Color` namespace's PascalCase convention.
- [x] 6.2 Update the three `src/support/` files that include `"color.h"` (`SmoothBlend.cpp`, `Color.cpp` itself, `Palette.cpp`) to include `"Color.h"`. Re-run `pio test -e native` to confirm 134/134 still green.