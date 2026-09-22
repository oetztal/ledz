## Context

`scripts/show_variants.json` is the canonical store for curated show presets and factory defaults. `scripts/gen_show_variants.py` turns it into two artifacts: a C++ header (`src/generated/show_variants.h`, consumed by the firmware and touch controller) and a JS payload (`src/generated/show_variants.js`), which `scripts/compress_web.py` inlines into `data/control.html` at the `<!--SHOW_VARIANTS_INLINE-->` marker.

Today the JS payload is shaped as `SHOW_VARIANTS_BY_SHOW = { Show: { variant: params } }` plus `FLAG_PRESETS = { flag: params }`. On the control page, only two shows surface their variants, through hand-coded `<select>` elements and loader functions:

- `loadMandelbrotPreset()` writes `mandelbrotCre0`, `mandelbrotCim0`, … from `SHOW_VARIANTS_BY_SHOW.Mandelbrot`.
- `loadWavePreset()` writes `waveDecay`, `waveBrightnessFreq`, `waveMode` from `SHOW_VARIANTS_BY_SHOW.Wave`.

Both populate form fields and require the user to press "Apply Parameters". Nine shows have curated variants and no picker at all. The manifest's per-variant `label` (e.g. "Deep zoom on cardioid cusp") never reaches the browser, so the dropdowns hard-code their own labels.

## Goals / Non-Goals

**Goals:**
- One generic preset selector on the control page, rendered for every show that has curated variants in the manifest.
- Options and parameters sourced entirely from `SHOW_VARIANTS_BY_SHOW`; option text from the manifest `label`.
- Selecting a preset applies it immediately via `/api/show`.
- Retire the two hand-coded dropdowns, their loaders and their consts.
- Zero firmware/C++ change.

**Non-Goals:**
- Solid's bespoke preset buttons (flags, warm-white, gradient) are left alone; Solid is excluded from the generic selector.
- The NVS user-preset store (`/api/presets`), timers and touch control are untouched.
- No unification of curated variants with user presets.
- Shows with no curated variants (ColorRun, Jump) get no selector.

## Decisions

### Payload entries become `{ label, params }`

`SHOW_VARIANTS_BY_SHOW` is reshaped so each entry carries its manifest label alongside its params:

```js
const SHOW_VARIANTS_BY_SHOW = {
  "Mandelbrot": {
    "default":   { "label": "Default",                    "params": { ... } },
    "deep-zoom": { "label": "Deep zoom on cardioid cusp", "params": { ... } }
  }
};
```

`gen_show_variants.py` already reads `label` (`v.get("label", v_name)`) for the C++ header; the JS emitter reuses it. The synthetic `default` entry gets the label `"Default"`.

*Alternatives considered:* a sibling `SHOW_VARIANT_LABELS_BY_SHOW` map (additive, no shape change) — rejected because it splits one concept across two payloads and would grow a second lookup at every use. The only runtime consumer of `SHOW_VARIANTS_BY_SHOW` is `data/control.html`, which this change rewrites, so the reshape is cheap.

`FLAG_PRESETS` keeps its `{ flag: params }` shape; the flag buttons are unaffected.

### One selector per show params-section, rendered from data

Each show's parameter block is a `.params-section` toggled by `updateParameterVisibility()`. Hosting sections gain a `data-variants-for="<ShowName>"` attribute. On page init, a generic function walks `[data-variants-for]`, looks up `SHOW_VARIANTS_BY_SHOW[show]`, and — when the show has at least one variant and is not Solid — inserts a `<select class="preset-selector">` as the section's first child. Options are `default` first, then each variant, labelled from the manifest.

This keeps the selector markup in one place instead of hand-writing a `<select>` into every section, and avoids deriving section ids from show names (which are ad-hoc: `theaterChaseParams`, `morseCodeParams`).

### Apply on select

`onchange` reads `SHOW_VARIANTS_BY_SHOW[show][key].params` and `POST`s `{ "name": show, "params": <params> }` to `/api/show`. This is the same endpoint the existing per-show "Apply Parameters" buttons call.

*Rationale:* the existing populate-then-Apply pattern needs a per-show key→DOM-id map, which is exactly the coupling we are removing. Applying through the API needs no field mapping and handles Solid's colour arrays server-side. The manual param fields and their Apply buttons stay for fine-tuning after a preset is applied.

*Trade-off:* applying a preset no longer populates the form fields; the fields refresh on the next status poll. The apply handler sets `lastPopulatedShow = null` and `pendingParameterConfig = false` so the next `updateStatus()` repopulates them promptly.

### Selection is cleared on status refresh

The two retired dropdowns are cleared inside the `Wave` and `Mandelbrot` branches of `updateStatus()` so the picker reflects that the device state may have changed externally. The generic equivalent clears every `.preset-selector` at the start of `updateStatus()`, replacing both branch-local resets.

### Retire the hand-coded dropdowns

Deleted from `data/control.html`: the `mandelbrotPreset` and `wavePreset` `<select>` blocks and their `param-row`s, `loadMandelbrotPreset()` and `loadWavePreset()`, the `mandelbrotPresets` and `wavePresets` consts, and the two per-branch reset lines. The Wave branch's `updateWaveBrightnessFreqHint()` call is retained.

## Risks / Trade-offs

- **Reshaping the generated payload breaks any unnoticed consumer** → `SHOW_VARIANTS_BY_SHOW` is referenced only in `data/control.html` and `scripts/gen_show_variants.py`; both are updated here. A repo-wide grep gates the change.
- **Form fields silently diverge from what was applied** → the apply handler triggers a status refresh, and `updateStatus()` remains the single source that populates fields.
- **`data-variants-for` drifts out of sync with `SHOW_VARIANTS_BY_SHOW` keys** (a show gets a section but no manifest entry, or vice versa) → a missing manifest entry simply yields no selector; the selector is purely additive, so drift degrades to today's behaviour rather than breaking the page.
- **Spec churn in `canonical-show-config`** → the delta updates the payload-shape requirement and replaces the Mandelbrot/Wave dropdown scenarios with the generic-selector behaviour; the rest of the capability is untouched.
- **Reduced discoverability of the retired labels** ("🔍 Deep Zoom") → manifest labels are used verbatim; if a manifest label is missing, the script falls back to the variant name.

## Migration Plan

The generated JS and header are rebuilt on every PlatformIO build (`pre:` hook), so no manual migration is required. The web payload is baked into `control_gz.h` at build time. Rollback is reverting the source change and rebuilding.
