## Why

After three archived changes — `canonical-show-config-file`, `converge-morse-default`, `consume-show-variants-in-firmware` — `scripts/show_variants.json` is the runtime source for `ShowFactory::createShow`, for the touch controller's Solid single-colour presets, and for the gallery renderer. The native parity test in `test/test_show_factory/test_show_factory.cpp` asserts byte-equality between the firmware's empty-params output and the JSON's `default.params` for all 12 registered shows. Two surfaces still declare default values independently of the JSON, and one of them is real drift:

1. **Constructor default arguments in `src/show/*.h`** — eight headers carry `= value` defaults on the parameters `ShowFactory` populates from `doc["key"] | value`. One real drift (`Starlight.h:47`'s `0.01f` vs JSON's `0.1`); seven coincidental matches. These defaults are never invoked in production (`ShowFactory` is the only production caller and always passes explicit values). `test/test_shows/test_shows.cpp` also constructs shows directly and relied on these defaults; that file is updated as part of this change to supply explicit values for every parameter.
2. **Hand-maintained preset dictionaries in `data/control.html`** — `mandelbrotPresets` (8 entries) and `wavePresets` (3 entries) are JS object literals that mirror JSON entries. The three flag button functions (`loadUkraineFlag`, `loadItalianFlag`, `loadFrenchFlag`) hard-code colour values that duplicate `Solid.variants[ukraine]`/`italy` and a non-existent `france` entry. The web UI is what users see; any divergence between the JSON and the UI is the highest-impact remaining leak.

This change closes both leaks. The JSON becomes the single source of truth at every surface: C++ constructor signatures, the embedded C++ runtime table, and the rendered HTML's preset dropdowns and flag buttons.

## What Changes

- **Add `france` to `Solid.variants[]`** in `scripts/show_variants.json` so `loadFrenchFlag()` has a JSON entry to read from. Tricolour: blue `[[0,85,164]]`, white `[[255,255,255]]`, red `[[239,65,53]]`, with `"gradient": false`. Final `Solid.variants[]` is 14 entries (the existing 13 plus `france`).
- **Strip default arguments from every parameterised constructor in `src/show/*.h`.** Eight headers: `MorseCode.h:42`, `Starlight.h:47`, `Wave.h:42-43`, `Fire.h:53`, `Mandelbrot.h:16`, `TheaterChase.h:21`, `Stroboscope.h:27`, `Rainbow.h:13`. Every affected parameter becomes mandatory; the `ShowFactory` lambdas (the only production callers) already supply every argument explicitly. `test/test_shows/test_shows.cpp` constructs shows directly — those call sites are updated to supply explicit values. After this change, the C++ source no longer encodes default values for any parameter the JSON declares.
- **Extend `scripts/gen_show_variants.py`** to also emit `src/generated/show_variants.js`. The new file declares two top-level JS objects:
  - `const SHOW_VARIANTS_BY_SHOW = { "Mandelbrot": { "default": {...}, "deep-zoom": {...}, ... }, "Wave": {...}, ... }` — keys are show names; values are objects whose keys are variant names (including the synthetic `"default"`) and whose values are the corresponding `params` objects.
  - `const FLAG_PRESETS = { "ukraine": {...}, "italy": {...}, "france": {...} }` — the three flag entries from `Solid.variants[]`, exposed separately for the flag buttons.
- **Extend `scripts/compress_web.py`** to substitute a `<!--SHOW_VARIANTS_INLINE-->` marker in `data/control.html` with a `<script>` element whose body is the generated `show_variants.js` (read once, inlined into the HTML string before `minify_html()` runs). The minified+gzipped HTML then carries both the JS object and the page itself.
- **Replace `mandelbrotPresets`, `wavePresets`, and the three flag button bodies in `data/control.html`** with reads from the generated objects. The dropdowns and buttons behave identically from the user's perspective; the source of values is the JSON via the generated JS payload.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `canonical-show-config`: three amendments.
  - New requirement: **Constructors in `src/show/*.h` SHALL NOT carry default arguments for parameters the JSON declares defaults for.** The C++ header becomes a pure signature; default values live in the JSON. The Mandelbrot 1-ULP documented exception continues to apply to the `|` fallback chain in `ShowFactory.cpp` (defense-in-depth) but no longer applies to constructor signatures (which carry no defaults at all).
  - New requirement: **`data/control.html` preset dropdowns and flag buttons SHALL source their values from the JSON via the generated JS payload.** `mandelbrotPresets`, `wavePresets`, and the three flag buttons (`loadUkraineFlag`, `loadItalianFlag`, `loadFrenchFlag`) are removed from the HTML in favour of reads from `SHOW_VARIANTS_BY_SHOW.Mandelbrot`, `SHOW_VARIANTS_BY_SHOW.Wave`, and `FLAG_PRESETS` (the latter being the `Solid.variants[ukraine|italy|france]` entries exposed under a separate top-level name for the flag buttons).
  - Amended scenario in `Schema is consumable by Python and by future C++ / JS consumers`: add a scenario asserting that `gen_show_variants.py` emits `src/generated/show_variants.js` and that `compress_web.py` inlines it into the rendered HTML before gzip compression.

## Impact

- `scripts/show_variants.json`: 1 new entry (`Solid.france`). 13 → 14 entries in `Solid.variants[]`.
- `src/show/*.h`: 8 headers, ~16 lines of constructor signatures edited. No new code, no new dependencies.
- `test/test_shows/test_shows.cpp`: 7 direct construction sites (Fire, Rainbow × 4, Wave × 3) updated to supply explicit values for every parameter, because those sites relied on the defaults this change removes.
- `src/show/factory/ShowFactory.cpp`: unchanged. The factory lambdas' `|` fallbacks remain as defense-in-depth (unchanged from the `consume-show-variants-in-firmware` change).
- `scripts/gen_show_variants.py`: extended to emit a second file (`src/generated/show_variants.js`). The existing C++ emit path is unchanged.
- `src/generated/show_variants.js`: new file, ~3 KB raw / ~1 KB minified. Not tracked in git — regenerated on every PlatformIO build by `pre:scripts/gen_show_variants.py`, living under the gitignored `src/generated/` directory. This matches the existing `*_gz.h` pattern.
- `scripts/compress_web.py`: extended to substitute the `<!--SHOW_VARIANTS_INLINE-->` marker before `minify_html()` runs. One new substitution, ~10 lines of Python.
- `data/control.html`: ~30 lines of JS deleted (`mandelbrotPresets`, `wavePresets`, three `load*Flag` function bodies) and replaced with reads from the generated `SHOW_VARIANTS_BY_SHOW` / `FLAG_PRESETS`. One new `<!--SHOW_VARIANTS_INLINE-->` marker.
- `src/generated/control_gz.h`: regenerated as part of every ESP32 build, byte content changes (because the JS payload is now part of the page).
- Firmware image size: +~3 KB for the ESP32 binary (the inlined JS payload). The C++ constructor defaults being removed has no measurable effect on code size.
- No new HTTP endpoint. No NVS schema change. No `ShowFactory` API change. No `POST /api/show` behavioural change for any show. `Solid.france` becomes selectable from the web UI and from the gallery (new preview PNG) — it was not previously reachable from either surface because no JSON entry existed.
- `docs/show_previews/`: gains one new PNG (`Solid_france.png`) on the next `python3 scripts/build_pages.py --all docs/show_previews --landing --seed 42` invocation. All other PNGs are byte-identical because no other `default.params` or `variants[].params` changes.
- `docs/SHOW_PARAMETERS.md` flag examples (`[[0, 87, 183], [255, 215, 0]]` etc.): **not modified by this change**. Generating them from the JSON would require a third sink for the same data (`.md` text) with no existing build hook for docs. Deferred to a follow-up.
- `src/TouchController.cpp`'s `TOUCH_ONLY_*` arrays (the 10 hand-coded touch presets for non-Solid shows): **not modified by this change**. The `consume-show-variants-in-firmware` design explicitly named these as touch-UX-curated and out of scope; revisiting that boundary is a separate decision.
