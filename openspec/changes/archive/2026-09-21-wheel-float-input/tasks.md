## 1. Shared wheel function

- [x] 1.1 Change the `wheel()` declaration in `src/color.h` from `unsigned char` to `float`. No other header content changes.
- [x] 1.2 Replace the `wheel()` body in `src/color.cpp`. Handle `NaN` and `±Inf` first by returning `0x000000`. Wrap all other finite inputs into `[0, 255)` via `h = fmodf(h, 255.0f); if (h < 0) h += 255.0f;`. Clamp the rare `[254, 255)` case to 254. Then compute `pos = h * 6.0f / 255.0f`, `section = static_cast<int>(pos)`, `frac = pos - section`. Replace the six integer branches with the float generalization (same six sectors, channel computed as `255 * frac`, `255 * (1 - frac)`, or `0`). Update the inline comment to reflect the new behaviour and to reference the byte-identical-at-integer-inputs guarantee.

## 2. Show callers

- [x] 2.1 In `src/show/Rainbow.cpp`, replace the line that computes `uint8_t hue_index = static_cast<uint8_t>(fmodf(hue_position, 255.0f));` and the `wheel(hue_index)` call with a single `strip.setPixelColor(index, wheel(hue_position));` — wheel now wraps internally, so the local `fmodf` is no longer needed. Drop the `static_cast` line entirely.
- [x] 2.2 In `src/show/Wave.cpp`, replace the `int color_index_raw = static_cast<int>(emission_time * 20.0f) % 255; if (color_index_raw < 0) color_index_raw += 255; uint8_t color_index = static_cast<uint8_t>(color_index_raw);` block and the `wheel(color_index)` call with a single `Strip::Color pixel_color = wheel(emission_time * 20.0f);` line. Wheel handles negative-emission-time wrap internally.
- [x] 2.3 In `src/show/TheaterChase.cpp`, replace the `uint8_t color_index = (uint8_t)(cycle_position * 255.0f);` line and the `wheel(color_index)` call with a single `Strip::Color chase_color = wheel(cycle_position * 255.0f);` line.

## 3. Tests

- [x] 3.1 Add `test_wheel_byte_identical_at_integer_inputs` to `test/test_color/test_color.cpp`. Loop `h` over `[0, 254]` and assert that `wheel((float)h)` produces the same `Strip::Color` as the documented sector-output formula the existing integer body implements (e.g. `((h <= 42) ? 0xFF0000 | (h*6 << 8) : ...)`). The single test pins the entire 255-position contract.
- [x] 3.2 Add `test_wheel_pure_red_green_blue` asserting `wheel(0.0f) == 0xFF0000`, `wheel(85.0f) == 0x00FF00`, `wheel(170.0f) == 0x0000FF` — the three primary anchors.
- [x] 3.3 Add `test_wheel_pure_yellow_cyan_magenta` asserting `wheel(42.5f) == 0xFFFF00`, `wheel(127.5f) == 0x00FFFF`, `wheel(212.5f) == 0xFF00FF` — the three mid-sector colours the integer version couldn't reach.
- [x] 3.4 Add `test_wheel_wraps_at_full_revolutions` asserting `wheel(255.0f) == wheel(0.0f)` and `wheel(510.0f) == wheel(0.0f)`.
- [x] 3.5 Add `test_wheel_wraps_negative_inputs` asserting `wheel(-1.0f) == wheel(254.0f)` and `wheel(-256.0f) == wheel(0.0f)`.
- [x] 3.6 Add `test_wheel_clamps_sub_degree_hue` asserting `wheel(254.7f) == wheel(254.0f)` — the only case where `h` is in `[254, 255)` after wrap.
- [x] 3.7 Add `test_wheel_nan_returns_black` asserting `wheel(NaN) == 0x000000`.
- [x] 3.8 Add `test_wheel_infinities_return_black` asserting `wheel(+Inf) == 0x000000` and `wheel(-Inf) == 0x000000`.

## 4. Verify

- [x] 4.1 `pio test -e native` builds clean and the full native suite is green (126 pre-existing tests + 8 new tests = 134 total).
- [x] 4.2 `pio test -e native -f test_shows` and `-f test_show_factory` both pass — these exercise the simplified Wave and Rainbow factory paths against the new `wheel(float)` signature.
- [x] 4.3 Visual sanity: with Rainbow at `time_step=1.0, pixel_step=1.0` (defaults), the rendered output is byte-identical to the prior `wheel(unsigned char)` behaviour. Verified by `test_wheel_byte_identical_at_integer_inputs`: when both time_step and pixel_step are integer 1.0, `hue_position = iteration + index` is always an integer, so the byte-identical-at-integers guarantee covers Rainbow defaults directly. The full 134/134 native test result confirms this end-to-end (Rainbow, Wave, and TheaterChase all still produce their pre-existing expected pixels via the simplified call sites).
