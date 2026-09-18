## 1. Update Wave implementation

- [x] 1.1 Update `src/show/Wave.h`: change default constructor parameters to `decay_rate=1.0, brightness_frequency=0.07, wavelength=15.0`; document the new "per wavelength" semantic for `decay_rate` and the end-fade behaviour.
- [x] 1.2 Update `src/show/Wave.cpp`: replace per-strip-length decay (`/ N`) with per-wavelength decay (`/ wavelength`); add the end-fade factor `1 - 2 * |source_pos / (N - 1) - 0.5|` to the source brightness.
- [x] 1.3 Update `src/ShowFactory.cpp` Wave registration: use the new default values for the three parameters.

## 2. Documentation

- [x] 2.1 Update `docs/SHOW_PARAMETERS.md` Wave section: reflect the new default values and the new "per wavelength" semantic for `decay_rate`.

## 3. Verification

- [x] 3.1 Build for ESP32 target: `pio run -e adafruit_qtpy_esp32s3_nopsram` succeeds (RAM 18.7%, Flash 66.9%, no per-frame allocations introduced).
- [x] 3.2 Run native tests: `pio test -e native` — all 123 tests pass, including the four Wave tests in `test_shows` and the three Wave tests in `test_show_factory`. Test fix required: `test_wave_symmetric_lighting_around_mid_source` previously assumed `execute(strip, 10)` would land the source at mid-strip, but `time` advances by 0.05 per call, not per iteration — corrected to call execute ten times before asserting.
- [x] 3.3 Confirm no new per-pixel allocations are introduced in the updated `Wave.cpp` `execute()` (review against the no-allocation pattern in `Rainbow.cpp` / `Fire.cpp`); per-pixel call counts remain: one `sin`, one `exp`, one `fabs`, one `wheel`.