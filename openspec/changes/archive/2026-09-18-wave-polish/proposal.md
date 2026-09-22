## Why

After `wave-interference` shipped, the Wave show was visually convincing on a few test strips but the defaults and the decay formula left two visible problems: the strip felt under-lit on typical lengths (the per-strip-length decay made the far end almost black), and the cosine source produced a hot spot whenever it lingered at an extreme. This change updates the spec to reflect the implementation that was tuned to fix those two problems — the decay formula and the defaults are normative and currently out of sync with the code.

## What Changes

- `decay_rate` is reinterpreted: it now measures exponential decay per *wavelength* of distance from the source, not per unit of strip length. The same `decay_rate` produces the same visual falloff on a 30-pixel strip as on a 144-pixel strip.
- Default parameter values change from `2.0 / 0.1 / 6.0` to `1.0 / 0.07 / 15.0` (more decay headroom on long strips, slower bounce, longer wavelength).
- A new behaviour: source brightness fades to zero as the source approaches either end of the strip, removing the hot spot caused by the cosine source having zero velocity at the extremes.

No new parameters, no UI changes, no protocol changes. Existing NVS configs continue to load (missing fields fall through ArduinoJson's `|` operator to the new defaults; `wave_speed` continues to be silently ignored).

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `wave-show`: decay formula changes from `exp(-decay_rate * |i - source| / N)` to `exp(-decay_rate * |i - source| / wavelength)`. Default values for all three parameters change. A new requirement is added for the end-fade source-brightness behaviour.

## Impact

- **Code**: `src/show/Wave.cpp` (already updated), `src/show/Wave.h` (defaults already updated), `src/ShowFactory.cpp` (defaults already updated).
- **Docs**: `docs/SHOW_PARAMETERS.md` (already updated).
- **Tests**: `test/test_shows/test_shows.cpp` and `test/test_show_factory/test_show_factory.cpp` — no test changes needed because they don't assert on specific default values or decay semantics.
- **Existing users**: anyone with Wave configured sees different (richer) behavior on next show change or reboot because the new defaults are picked up. Existing parameter values in NVS continue to be honoured — only the fall-back defaults change.
- **API**: `POST /api/show` accepts the same three parameters. NVS-stored configs that omit `decay_rate` now use `1.0` (was `2.0`); omitting `brightness_frequency` now uses `0.07` (was `0.1`); omitting `wavelength` now uses `15.0` (was `6.0`).
