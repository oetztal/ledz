## 1. Generated JS payload carries labels

- [x] 1.1 In `scripts/gen_show_variants.py`, update `build_js_payload()` so each `SHOW_VARIANTS_BY_SHOW` entry is an object `{ "label": <label>, "params": <params> }` instead of a bare params object. The synthetic `default` entry SHALL use the label `"Default"`; each variant SHALL use its manifest `label` (which the script already resolves with `v.get("label", v_name)`). `FLAG_PRESETS` SHALL keep its plain `{ flag: params }` shape.
- [x] 1.2 Run `python3 scripts/gen_show_variants.py` and confirm `src/generated/show_variants.js` declares `SHOW_VARIANTS_BY_SHOW` entries in `{ label, params }` form and unchanged `FLAG_PRESETS`.
- [x] 1.3 Confirm the JS parses: `node -e "require('./src/generated/show_variants.js')"` (or equivalent) reports no error, and `SHOW_VARIANTS_BY_SHOW.Mandelbrot['deep-zoom'].label` equals `"Deep zoom on cardioid cusp"`.

## 2. Retirement of hand-coded dropdowns

- [x] 2.1 In `data/control.html`, delete the `mandelbrotPreset` `<select>` block (the `param-row` containing `for="mandelbrotPreset"` and its `loadMandelbrotPreset(this.value)` `onchange`).
- [x] 2.2 In `data/control.html`, delete the `wavePreset` `<select>` block (the `param-row` containing `for="wavePreset"` and its `loadWavePreset(this.value)` `onchange`).
- [x] 2.3 Delete the `const mandelbrotPresets = SHOW_VARIANTS_BY_SHOW.Mandelbrot;` and `const wavePresets = SHOW_VARIANTS_BY_SHOW.Wave;` declarations.
- [x] 2.4 Delete the `loadMandelbrotPreset(value)` and `loadWavePreset(value)` functions.
- [x] 2.5 In `updateStatus()`, delete the `document.getElementById('wavePreset').value = '';` line in the `Wave` branch and the `document.getElementById('mandelbrotPreset').value = '';` line in the `Mandelbrot` branch. Keep the `updateWaveBrightnessFreqHint()` call in the `Wave` branch.
- [x] 2.6 Confirm no references remain: `grep -nE 'mandelbrotPreset|wavePreset|mandelbrotPresets|wavePresets|loadMandelbrotPreset|loadWavePreset' data/control.html` returns nothing.

## 3. Generic preset selector

- [x] 3.1 In `data/control.html`, add a `data-variants-for="<ShowName>"` attribute to each show parameter section that has curated variants: `fireParams` (Fire), `starlightParams` (Starlight), `stroboscopeParams` (Stroboscope), `rainbowParams` (Rainbow), `waveParams` (Wave), `theaterChaseParams` (TheaterChase), `morseCodeParams` (MorseCode), `chaosParams` (Chaos), `mandelbrotParams` (Mandelbrot). Do NOT add it to `colorRangesParams` (Solid) or to any show with no variants.
- [x] 3.2 Add `initPresetSelectors()`: for each `[data-variants-for]` element, read the show name, look up `SHOW_VARIANTS_BY_SHOW[showName]`; skip when the lookup is absent or has no keys other than `default`; otherwise build a `<select class="preset-selector">` with a placeholder option, then `default` (labelled from its entry, expected `"Default"`), then every variant, and insert it as the section's first child. Use each entry's `label` for option text, falling back to the key when `label` is absent.
- [x] 3.3 Add the selector's change handler: read `SHOW_VARIANTS_BY_SHOW[showName][key].params` and send `POST /api/show` with `{"name": showName, "params": <params>}`. After the request, set `pendingParameterConfig = false` and `lastPopulatedShow = null` so the next `updateStatus()` repopulates the parameter fields.
- [x] 3.4 Call `initPresetSelectors()` from `init()` after `loadShows()` / `updateStatus()`.
- [x] 3.5 At the start of `updateStatus()`, reset every `.preset-selector` back to its placeholder option so a status refresh does not imply the last-selected preset is still active.

## 4. Verification

- [x] 4.1 Build firmware so the `pre:` hook regenerates artifacts: `pio run -e adafruit_qtpy_esp32s3_nopsram`.
- [x] 4.2 Confirm the embedded page carries the selector: decompress `control_gz.h` (or inspect the built HTML) and check it contains `data-variants-for` and `preset-selector`.
- [x] 4.3 Confirm the retired identifiers are gone from the shipped HTML: no `mandelbrotPreset`, `wavePreset`, `loadMandelbrotPreset`, `loadWavePreset`, `mandelbrotPresets`, or `wavePresets`.
- [x] 4.4 On the device (or a local render), verify every show with curated variants (Fire, Starlight, Stroboscope, Rainbow, Wave, TheaterChase, MorseCode, Chaos, Mandelbrot) shows a preset selector with manifest labels plus a leading `Default`, and that selecting one applies immediately.
- [x] 4.5 Verify Solid shows no generic selector and keeps its flag / warm-white / gradient buttons; verify ColorRun and Jump show no selector.
- [x] 4.6 Run the native tests to confirm no regression: `pio test -e native`.