## 1. Refresh Wave implementation

- [x] 1.1 Update `src/show/Wave.h`: change constructor signature to drop `wave_speed`, document new parameter semantics (decay from oscillating source, bounce frequency, wavelength).
- [x] 1.2 Rewrite `src/show/Wave.cpp`: implement cosine-bouncing source, distance-from-source exponential decay, signed `sin` with `fabs` for brightness, retain the existing `wheel()` color cycle based on emission time.
- [x] 1.3 Update `src/ShowFactory.cpp` Wave registration: parse only `decay_rate`, `brightness_frequency`, `wavelength`; drop the `wave_speed` parse but do not error if present in JSON.
- [x] 1.4 Update the Wave human-readable description in `src/ShowFactory.cpp` to describe bouncing source / interference rather than end-emanating waves.

## 2. Tests

- [x] 2.1 Add a default-constructor smoke test in `test/test_shows/test_shows.cpp`: construct `Wave` with no args, execute for several iterations against `MockStrip`, assert no crash and strip length is preserved.
- [x] 2.2 Add an explicit-constructor smoke test: construct `Wave` with non-default parameters, execute, assert no crash.
- [x] 2.3 Add a symmetric-lighting test: at an iteration where `source_pos` is near the middle of the strip, assert that pixels on both sides of the source have non-black color (proves the new decay shape works).
- [x] 2.4 Add a JSON-parsing test in `test/test_show_factory/test_show_factory.cpp`: feed `Wave` a JSON string with all three fields, assert the factory constructs a Wave with those values (mirrors the existing Rainbow factory test).

## 3. Documentation

- [x] 3.1 Update `docs/SHOW_PARAMETERS.md` Wave section: describe the new behavior (bouncing source, reflection, interference), document the three parameters with their new meanings, document that `wave_speed` is no longer accepted.

## 4. Verification

- [x] 4.1 Build for native: `pio run -e native` succeeds.
- [x] 4.2 Run native tests: `pio test -e native` passes including the new Wave tests.
- [x] 4.3 Build for ESP32 target: `pio run -e adafruit_qtpy_esp32s3_nopsram` succeeds.
- [x] 4.4 Confirm no new per-pixel allocations are introduced in the new `Wave.cpp` `execute()` (review against the no-allocation pattern in `Rainbow.cpp` / `Fire.cpp`).
