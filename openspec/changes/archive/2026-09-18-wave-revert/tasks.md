## 1. Revert Wave implementation

- [x] 1.1 `src/show/Wave.h`: revert default constructor parameters to `decay_rate=2.0, brightness_frequency=0.1, wavelength=6.0`; revert the class-level comment block to the wave-interference wording.
- [x] 1.2 `src/show/Wave.cpp`: restore per-strip-length decay `exp(-decay_rate * abs_distance * inv_num_leds)`; remove the end-fade factor; revert the per-loop comment to the wave-interference wording.
- [x] 1.3 `src/ShowFactory.cpp` Wave registration: revert default values to `decay_rate=2.0, brightness_frequency=0.1, wavelength=6.0`.

## 2. Revert documentation

- [x] 2.1 `docs/SHOW_PARAMETERS.md`: revert the Wave section to the wave-interference text — defaults `2.0 / 0.1 / 6.0`, decay semantic "per unit of strip length", and example JSON reflecting those defaults.

## 3. Verification

- [x] 3.1 Run native tests: `pio test -e native` passes all 123 tests (including the seven Wave tests in `test_shows` and `test_show_factory`).
- [x] 3.2 Build for ESP32 target: `pio run -e adafruit_qtpy_esp32s3_nopsram` succeeds with no new warnings.
- [x] 3.3 Confirm `test_wave_symmetric_lighting_around_mid_source` still passes against the reverted math (the test fix from wave-polish — calling `execute` ten times to advance `time` to 0.5 s — is preserved; the reverted math gives even brighter pixels near the source so the assertions hold).