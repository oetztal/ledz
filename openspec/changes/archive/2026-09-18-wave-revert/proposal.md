## Why

`wave-polish` shipped three structural changes (per-wavelength decay, end-fade brightness factor, and slower defaults `1.0 / 0.07 / 15.0`) that were hypothesised to make the show more convincing. After watching it on real strips, the result is visibly worse than the `wave-interference` version it replaced: peak brightness drops ~44 %, mean brightness across the strip drops ~42 %, and the bright region is roughly half the size it used to be. The "polish" was a regression.

This change reverts `wave-polish` so the code, docs, and normative spec all return to the `wave-interference` state.

## What Changes

- Revert `decay_rate` default from `1.0` back to `2.0`, `brightness_frequency` default from `0.07` back to `0.1`, `wavelength` default from `15.0` back to `6.0`.
- Revert the decay formula from `exp(-decay_rate * |i - source_pos| / wavelength)` back to `exp(-decay_rate * |i - source_pos| / N)` (per-strip-length).
- Remove the end-fade brightness factor entirely.
- Update the docs to describe the original (per-strip-length) decay semantic and the original defaults.

No NVS migration is needed: the field names stay the same, only the fall-back defaults change, and ArduinoJson's `|` operator handles missing fields on existing configs without error. NVS-stored configs with explicit values keep those values.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `wave-show`: decay formula changes from per-wavelength back to per-strip-length. Default values for all three parameters change. The "Source brightness fades near the strip ends" requirement is removed (the end-fade behaviour is gone).

### Removed Capabilities

None.

## Impact

- **Code**: `src/show/Wave.h` (defaults reverted, comments reverted), `src/show/Wave.cpp` (per-strip-length decay restored, end-fade removed), `src/ShowFactory.cpp` (defaults reverted).
- **Docs**: `docs/SHOW_PARAMETERS.md` (Wave section restored to the wave-interference text).
- **Tests**: no test changes. The `test_wave_symmetric_lighting_around_mid_source` test in `test/test_shows/test_shows.cpp` already calls execute ten times to advance time to 0.5 s (a fix applied during wave-polish); that fix is preserved because it more accurately tests the comment's intent, and the test passes against the reverted math.
- **Existing users**: anyone whose stored config falls back to defaults gets the wave-interference defaults back. Anyone with explicit stored parameter values keeps those values either way. Visual behaviour returns to what was on the device between the wave-interference and wave-polish archives.
- **API**: `POST /api/show` still accepts the same three parameters with the same field names. NVS-stored configs that omit `decay_rate` again use `2.0` (was `1.0`); omitting `brightness_frequency` again uses `0.1` (was `0.07`); omitting `wavelength` again uses `6.0` (was `15.0`).
