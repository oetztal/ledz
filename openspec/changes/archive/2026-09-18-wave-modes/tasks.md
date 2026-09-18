## 1. C++ implementation

- [x] 1.1 Add `enum class WaveMode { Bounce, Traveling }` to `src/show/Wave.h`. Add a `mode` member field. Extend the constructor to take `WaveMode mode = WaveMode::Bounce` as the fourth parameter (after the existing three floats).
- [x] 1.2 In `src/show/Wave.cpp` `execute()`, replace the single phase formula with a mode-aware one. For `Bounce`, keep `2π · (i − source_pos) / λ`. For `Traveling`, use `2π · (i/λ − t · freq)`. All other per-pixel computations (envelope, hue, source brightness) stay identical. No new `sin`/`exp`/`fabs`/`wheel` calls per pixel.
- [x] 1.3 In `src/ShowFactory.cpp`, parse the `mode` JSON field with `doc["mode"] | "bounce"` and convert to `WaveMode` (unknown string → `Bounce`). Pass it to the `Wave` constructor.
- [x] 1.4 In `src/ShowFactory.cpp`, update the Wave `registerShow` description to mention both modes (replace the current "bouncing rainbow source..." text with the new two-mode text from `design.md` Decision 5).
- [x] 1.5 Confirm the four existing `Wave` tests in `test/test_shows/test_shows.cpp` still pass with `mode = Bounce` as the default. No test changes required.

## 2. UI changes (`data/control.html`)

- [x] 2.1 Inside the `waveParams` div (around line 208 of `data/control.html`), remove the `waveSpeed` `<input>` block (the entire `<div class="param-row">` containing `Wave Speed`, its `waveSpeed` input, and the "Higher = faster wave propagation" hint).
- [x] 2.2 Inside the same `waveParams` div, add a `waveMode` `<select>` block as the first `param-row`. Two `<option>` elements: `value="bounce"` labelled `Bounce`, `value="traveling"` labelled `Traveling`. Default selected = `bounce`. Add a short hint (`<small>`) explaining what each mode does.
- [x] 2.3 Update the `applyWaveParams()` JS function (around line 645 of `data/control.html`): stop reading `waveSpeed`; read the new `waveMode` select's value; POST `{name: "Wave", params: {mode, decay_rate, brightness_frequency, wavelength}}`.
- [x] 2.4 Update the `case 'Wave'` branch of `populateShowParams()` (around line 871): stop reading `params.wave_speed`; read `params.mode` and set the `waveMode` `<select>` accordingly; keep reading `decay_rate`, `brightness_frequency`, `wavelength` (unchanged).
- [x] 2.5 Add a small JS handler that, when the user changes `waveMode`, swaps the `<small>` help text under the brightness-frequency input: `Bounce` says "Bounces per second", `Traveling` says "Phase cycles per second — drift = freq × wavelength px/s".
- [x] 2.6 Verify the build regenerates `src/generated/control_gz.h` from `data/control.html`. (`pio run -e adafruit_qtpy_esp32s3_nopsram` invokes the data pipeline; confirm the build emits the new file with the updated content.)

## 3. Documentation

- [x] 3.1 In `docs/SHOW_PARAMETERS.md`, update the Wave section to document the new `mode` parameter (default `"bounce"`, values `"bounce"` and `"traveling"`). Document both behaviors and add an example JSON for traveling.
- [x] 3.2 Note that `wave_speed` is removed from the UI and is no longer documented as an input. (The factory's silent-ignore behaviour for any pre-existing `wave_speed` field in NVS is unchanged.)

## 4. Tests

- [x] 4.1 Add `test_wave_traveling_mode_stripes_drift_independent_of_source` to `test/test_shows/test_shows.cpp`: construct a `Wave(2.0f, 0.1f, 6.0f, WaveMode::Traveling)`, execute on a 60-LED strip for a known number of iterations, and assert that the position of a chosen stripe shifts by the expected number of pixels (≈ `0.1 × 6 × iterations × 0.05`).
- [x] 4.2 Add `test_wave_parses_mode_from_json` to `test/test_show_factory/test_show_factory.cpp`: POST-style JSON `"{\"mode\":\"traveling\",\"decay_rate\":2.0,\"brightness_frequency\":0.1,\"wavelength\":6.0}"` and confirm the constructed show's mode is `WaveMode::Traveling`.
- [x] 4.3 Add `test_wave_unknown_mode_falls_back_to_bounce` to `test/test_show_factory/test_show_factory.cpp`: JSON `"{\"mode\":\"bogus\"}"` and confirm the constructed show's mode is `WaveMode::Bounce`.
- [x] 4.4 Confirm all existing tests still pass: `pio test -e native` reports 0 failures across all suites.

## 5. Build & verify

- [x] 5.1 `pio run -e native` builds clean.
- [x] 5.2 `pio run -e adafruit_qtpy_esp32s3_nopsram` builds clean.
- [x] 5.3 `pio test -e native` passes all suites (existing + new tests).
- [x] 5.4 `pio test -e native -f test_wave` runs the wave-specific suite with all new tests green.
- [ ] 5.5 (Optional, on-device) Upload to hardware and visually confirm: selecting Wave with default params looks identical to today; switching mode to Traveling in the UI shows drifting stripes inside the bouncing envelope.