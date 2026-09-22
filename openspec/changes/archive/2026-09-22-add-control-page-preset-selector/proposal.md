## Why

`scripts/show_variants.json` is the canonical store for curated show presets, but its variants reach the control page through two hand-coded dropdowns (Mandelbrot, Wave) and three flag buttons. Nine shows carry curated variants the UI never exposes — Fire, Starlight, Stroboscope, Rainbow, Wave, TheaterChase, MorseCode, Chaos and Mandelbrot. Every new variant or new show with variants requires touching `data/control.html` by hand. A single data-driven selector, generated from the same manifest, unlocks every curated preset at once and makes the manifest truly the only place presets are declared.

## What Changes

- Add one generic preset selector to the control page. It is rendered for the currently selected show whenever that show has curated variants in the manifest, and it applies the selected preset immediately (no separate Apply click).
- Include the show's factory `default` as a selectable entry alongside its variants.
- Source option labels and parameters from `SHOW_VARIANTS_BY_SHOW` — reshape the generated JS payload entries from `{ variant: params }` to `{ variant: { label, params } }` so the manifest's `label` reaches the browser.
- **BREAKING** (build-generated payload shape): the inlined JS object `SHOW_VARIANTS_BY_SHOW` changes value shape. Its only runtime consumer is `data/control.html`, updated in this change.
- Exclude Solid from the generic selector: its bespoke flag / warm-white / gradient buttons remain. Shows with no variants (ColorRun, Jump) get no selector.
- Retire the hand-coded `mandelbrotPreset` and `wavePreset` dropdowns, their `loadMandelbrotPreset()` / `loadWavePreset()` functions, the `mandelbrotPresets` / `wavePresets` consts, and the reset lines in `updateStatus()` that clear those selects.
- Non-goal: this does not touch the separate NVS user-preset store (`/api/presets`), timers, or touch control.

## Capabilities

### New Capabilities
- `control-page-preset-selector`: a data-driven preset selector on the control page, rendered per show from the curated variants in the manifest, including the factory default, applying immediately on selection.

### Modified Capabilities
- `canonical-show-config`: the generated `SHOW_VARIANTS_BY_SHOW` JS payload now carries `{ label, params }` per entry; the Mandelbrot/Wave dropdown scenarios are replaced by the generic selector behaviour.

## Impact

- `scripts/gen_show_variants.py` — emit `{ label, params }` entries in the JS payload.
- `src/generated/show_variants.js` (gitignored, regenerated on every build).
- `data/control.html` — remove the two hand-coded dropdowns, their loader functions and consts; add the generic selector component and its apply path.
- `openspec/specs/canonical-show-config/spec.md` — delta for the payload shape and the retired dropdown scenarios.
- No firmware C++ changes. `FLAG_PRESETS` and the generated C++ header are unchanged.
