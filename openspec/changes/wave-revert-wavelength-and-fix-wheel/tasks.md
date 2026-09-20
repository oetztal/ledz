## 1. Wave show header and implementation

- [ ] 1.1 Remove `wavelength` member, constructor parameter, and `wavelength` doc lines from `src/show/Wave.h`. Update the class doc comment to describe the wavelength-free behaviour.
- [ ] 1.2 Remove the wavelength-based phase computation and the `|sin(phase)|` wave amplitude from `src/show/Wave.cpp`. Per-pixel brightness becomes `source_brightness * envelope` only. The `mode` parameter is still accepted by the constructor and stored, but is unused inside `execute()`. Update the inline comments to reflect the new structure.

## 2. Shared wheel function

- [ ] 2.1 Replace the Adafruit edge-walking body of `wheel()` in `src/color.cpp` with an HSV cube-walking integer-arithmetic implementation that matches `scripts/wave_show.py`'s `wheel()` within ±1 across all 255 positions and produces pure yellow at ~42, cyan at ~127, and magenta at ~212. Signature and `[0, 254]` clamping behaviour stay the same.

## 3. Factory JSON contract

- [ ] 3.1 Drop the `wavelength` parse, the log-line wavelength field, and the constructor argument from the Wave registration in `src/show/factory/ShowFactory.cpp`. Keep `mode`, `decay_rate`, and `brightness_frequency`. Keep the existing comment that `wave_speed` is silently ignored.

## 4. Web UI

- [ ] 4.1 Remove the Wavelength input row from the Wave parameter section in `data/control.html`.
- [ ] 4.2 Drop the `wavelength` read/write from `applyWaveParams()` and from the `case 'Wave':` branch of `populateShowParams()` in `data/control.html`. Update the `updateWaveBrightnessFreqHint()` text since it no longer references wavelength.

## 5. Documentation

- [ ] 5.1 Rewrite the Wave section of `docs/SHOW_PARAMETERS.md` to drop `wavelength` from the parameter list, drop the wavelength example payloads, and note that `mode` is currently a no-op.

## 6. Tests

- [ ] 6.1 Update `test/test_shows/test_shows.cpp`: drop the wavelength argument from the `Show::Wave` constructions in `test_wave_explicit_constructor_does_not_crash` and `test_wave_symmetric_lighting_around_mid_source`; remove the wavelength-dependent `test_wave_traveling_mode_stripes_drift_independent_of_source` (no phase to drift without wavelength). Add a new `test_wave_bounce_and_traveling_modes_are_identical` that asserts both modes produce identical pixels for the same parameters.
- [ ] 6.2 Update `test/test_show_factory/test_show_factory.cpp`: rewrite `test_wave_parses_all_three_parameters_from_json` to drop `wavelength` from the payload and from the assertions; rewrite `test_wave_partial_parameters_use_defaults` to use one of the remaining parameters; rewrite `test_wave_ignores_wave_speed_legacy_field` to use a remaining parameter alongside `wave_speed`; rewrite `test_wave_parses_mode_from_json` to assert the two modes produce identical pixels; leave `test_wave_unknown_mode_falls_back_to_bounce` as-is.

## 7. Verify

- [ ] 7.1 `pio run -e native` builds clean.
- [ ] 7.2 `pio test -e native` runs the full native suite (all show factory, show, color, etc. tests) green.
- [ ] 7.3 Spot-check `scripts/wave_show.py --mode bounce` against a few manually-computed C++ pixel values (e.g. iteration 0, pixel 0 on a 10-LED strip with default parameters) to confirm the byte-for-byte alignment holds.
