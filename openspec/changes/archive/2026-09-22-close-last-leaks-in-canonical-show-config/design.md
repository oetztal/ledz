## Context

Three changes have been archived on the canonical-show-config trajectory:

```
canonical-show-config-file          converge-morse-default     consume-show-variants-in-firmware
(bfb6f13)                           (075fc6d)                  (4af0099)
──────────────────────────          ─────────────────────────  ──────────────────────────────────
• JSON: default vs variants split  • MorseCode in 4 places:   • scripts/gen_show_variants.py
• Every params fully materialised    JSON, factory, ctor, HTML  → src/generated/show_variants.h
• Renderer treats default as       • spec amended: 4-source   • ShowFactory merges JSON default
  synthetic first variant            contract                   + user payload via per-key copy
• 8 of 12 defaults move out of                                   • TouchController Solid 13 variants
  variants[] into default[]                                       •   come from generated header
                                                                 • native parity test: byte-equality
                                                                   for every show (Mandelbrot ULP
                                                                   doesn't surface at JSON layer)
```

`pio test -e native` is green (22/22). `pio run -e adafruit_qtpy_esp32s3_nopsram` is green. Working tree clean.

Two surfaces still declare default values independently of `scripts/show_variants.json`:

1. **Constructor default arguments in `src/show/*.h`** — eight headers (`MorseCode`, `Starlight`, `Wave`, `Fire`, `Mandelbrot`, `TheaterChase`, `Stroboscope`, `Rainbow`) carry `= value` defaults on parameters `ShowFactory` populates from `doc["key"] | value`. One real drift: `Starlight.h:47` declares `probability = 0.01f` while `Starlight.default.params.probability` is `0.1`. The other seven happen to match the JSON by coincidence. In production, `ShowFactory` is the only caller (verified by `grep -l 'new \+Show::\|new \+MorseCode\|new \+Fire\|…' src/`) and every construction supplies them explicitly via the merged `JsonDocument`. `test/test_shows/test_shows.cpp` also constructs shows directly (`new Show::Wave()`, `Show::Rainbow show;`, `new Show::Fire(...)`, etc.); those sites relied on the defaults and are updated in this change to supply explicit values.

2. **Hand-maintained preset dictionaries in `data/control.html`**:
   - `mandelbrotPresets` (8 entries: `default`, `deep-zoom`, `wide-overview`, `mid-zoom`, `mid-zoom-2`, `intermediate-zoom`, `spiral`) duplicates `Mandelbrot.default.params` and `Mandelbrot.variants[]` from the JSON.
   - `wavePresets` (3 entries: `default`, `tight`, `calm`) duplicates `Wave.default.params` and `Wave.variants[]`.
   - `loadUkraineFlag()`, `loadItalianFlag()`, `loadFrenchFlag()` hand-code colour literals that duplicate `Solid.variants[ukraine]`, `[italy]`, and a non-existent `[france]` entry.

   The web UI is the surface users actually interact with. Any divergence between the JSON and the HTML is the highest-impact remaining leak. The existing pipeline already produces gzipped web assets (`src/generated/*_gz.h`) on every ESP32 build via `pre:scripts/compress_web.py` in `platformio.ini:69`, so the build path is well-established.

## Goals / Non-Goals

**Goals:**

- Make `scripts/show_variants.json` the single source of truth for every default value the firmware, the simulator, the touch controller, and the web UI consult — across the C++ header boundary, the constructor signature boundary, and the rendered HTML boundary.
- Preserve all existing user-visible behaviour. Every `POST /api/show {name, params}` produces the same strip state before and after the change. Every dropdown option the user sees today is still there, with the same value.
- Preserve the build pipeline shape: `gen_show_variants.py` emits the C++ table (today) and the JS payload (new); `compress_web.py` continues to emit `control_gz.h` (now with the JS payload inlined).

**Non-Goals:**

- Removing the `|` fallback chain in `src/show/factory/ShowFactory.cpp`'s factory lambdas. They remain as defense-in-depth, exactly as the `consume-show-variants-in-firmware` change left them.
- Generating `docs/SHOW_PARAMETERS.md` flag examples from the JSON. This is a third sink (`.md` text) for the same data with no existing build hook for docs. Deferred to a follow-up.
- Revisiting `src/TouchController.cpp`'s `TOUCH_ONLY_*` arrays (the 10 hand-coded touch presets for non-Solid shows). The `consume-show-variants-in-firmware` design explicitly named these as touch-UX-curated and out of scope.
- Changing the byte-equality contract that the parity test asserts (`createShow(name, "{}")` byte-equals the JSON `default.params` for every show except `Mandelbrot`'s documented 1-ULP exception).
- Renaming or restructuring the existing Solid touch entries; only the new `france` entry is added to `Solid.variants[]`.

## Decisions

### 1. Constructor defaults: strip, don't sync

`src/show/*.h` constructor signatures lose every `= value` default argument for parameters the JSON declares defaults for. After the change:

- `MorseCode.h:42`: `MorseCode(const std::string& message, float speed = 0.5f, ...)` → `MorseCode(const std::string& message, float speed, ...)`. Every parameter becomes mandatory.
- The Starlight.h:47 drift (`0.01f` vs JSON `0.1`) is dissolved because the literal ceases to exist in C++.

**Why strip over sync**: Sync would preserve seven coincidental matches and update one drift to match — but it would leave the C++ source encoding the values, contradicting the JSON-as-source principle the prior changes established. Strip forces every caller to be explicit — production callers (`ShowFactory`) and test callers (`test/test_shows/test_shows.cpp`'s direct construction sites) — and removes any future drift surface between the constructor and the JSON. Every call site that needed updating has been updated; `pio run -e adafruit_qtpy_esp32s3_nopsram` and `pio test -e native` confirm zero compile errors and 137 passing test cases.

**Alternatives considered**:

- *Update each default to match JSON*: rejected — leaves the contradiction in place; the next refactor in `ShowFactory` re-creates drift.
- *Keep defaults but add a compile-time assertion that they match the JSON*: rejected — adds runtime cost and tooling for a problem that disappears with stripping.

### 2. JS payload emitted by the same script as the C++ header

`scripts/gen_show_variants.py` is extended to emit both `src/generated/show_variants.h` (existing) and `src/generated/show_variants.js` (new) in one run. The JS payload exposes two top-level objects:

```javascript
const SHOW_VARIANTS_BY_SHOW = {
    "Mandelbrot": { "default": {...}, "deep-zoom": {...}, ... },
    "Wave":       { "default": {...}, "tight": {...}, "calm": {...} },
    ...
};
const FLAG_PRESETS = {
    "ukraine": {"colors":[[0,87,183],[255,215,0]], "gradient":false},
    "italy":   {"colors":[[0,140,69],[255,255,255],[205,33,42]], "gradient":false},
    "france":  {"colors":[[0,85,164],[255,255,255],[239,65,53]], "gradient":false},
};
```

`SHOW_VARIANTS_BY_SHOW` keys are show names; values are objects whose keys are the synthetic `"default"` plus every entry in `variants[]`; values are the corresponding `params` objects. The `normalize_for_arduinojson()` helper (already in the script) is reused, so the JS values are serialised with the same ArduinoJson-compatible conventions (`10.0` → `10`, `0.5` stays).

`FLAG_PRESETS` is the three `Solid.variants[ukraine|italy|france]` entries under a separate top-level name. It's exposed separately rather than accessed via `SHOW_VARIANTS_BY_SHOW.Solid.ukraine` because the flag buttons are a UI affordance, not a show-selection affordance — they're labelled with a flag, not a show-name plus preset-name. Keeping them out of `SHOW_VARIANTS_BY_SHOW` makes the data model mirror the UI model.

**Why one script rather than two**: One source means one parse-and-emit pipeline. The C++ header and the JS payload are different projections of the same parsed data; emitting them in separate scripts would require either re-parsing the JSON twice (slow, risk of parser version skew) or factoring out a shared parsing library (premature). One script with two output files is the smallest surface that works.

**Alternatives considered**:

- *Sister script `gen_show_variants_js.py`*: rejected — duplicates parse logic; if the schema grows a field, two scripts need updating.
- *Bake JS literals into `compress_web.py` instead*: rejected — `compress_web.py` is about byte-level compression; encoding the JSON→JS projection into it would mix concerns and make the JS payload harder to test standalone.

### 3. Web UI reads the JS payload via marker substitution, not separate script load

`data/control.html` carries a `<!--SHOW_VARIANTS_INLINE-->` marker. `scripts/compress_web.py` reads `src/generated/show_variants.js` once, substitutes the marker with a `<script>` element containing the JS, and then runs the existing `minify_html()` pipeline. The minified+gzipped HTML therefore carries both the page and the data.

**Why marker substitution rather than `<script src="show_variants.js">`**: The web server has no filesystem. Every byte the browser sees is the contents of one of `src/generated/*_gz.h`, decompressed on the fly by `WebServerManager`. There is no path for a separate `.js` file to reach the browser without inventing a new HTTP handler and a new gzipped payload — duplication of `compress_web.py`'s mechanism for no benefit.

**Alternatives considered**:

- *New HTTP endpoint `/show_variants.js`*: rejected — adds a second payload, a second gzipped asset, a second decompression path, and a second lifecycle (caching headers, version stamp). One inlined payload is simpler.
- *Generate `show_variants.js` and have `compress_web.py` reference it via a synthetic `<script>` and a parallel gzipped payload*: rejected — same problem as above, with the additional cost of two decompression paths on the device.

### 4. `compress_web.py` injects before `minify_html()`

The substitution step runs once, before any of the existing minification (CSS, JS, whitespace) is applied. Reasons:

- The JS payload uses `,` and `{` and `}` heavily; `minify_css` would mangle it because `minify_css` operates on the content of `<style>` tags, and `minify_js` is conservative but does collapse whitespace, which would not affect correctness but does affect readability when debugging.
- The HTML minifier removes HTML comments (`<!--…-->`) but our marker is itself a comment. If we substituted after minification, the marker would already be gone. Substituting before ensures the marker is consumed.

The JS is then inlined into a `<script>` block, which `minify_js` already knows how to handle (it strips comments and collapses whitespace). The result is a minified script element whose contents match what `gen_show_variants.py` produced.

### 5. France flag colours

`Solid.france` uses the French tricolour as it appears on the French flag: blue `[[0, 85, 164]]`, white `[[255, 255, 255]]`, red `[[239, 65, 53]]`, with `"gradient": false` (sharp boundaries, matching the Ukraine and Italy variants). These are the official RGB values from `fr.wikipedia.org/wiki/Drapeau_de_la_France`.

## Risks / Trade-offs

- **[Risk]** Stripping constructor defaults breaks a non-`ShowFactory` caller → **Mitigation**: `grep -r 'new \+\(MorseCode\|Starlight\|Wave\|Fire\|Mandelbrot\|TheaterChase\|Stroboscope\|Rainbow\|Solid\|ColorRanges\)' src/ test/` covers every construction site. `src/` matches only `ShowFactory.cpp`; `test/test_shows/test_shows.cpp` has direct construction sites, all updated to supply explicit values for every parameter. A compiler error in any unexpected site would surface immediately on `pio run`.
- **[Risk]** `compress_web.py` substitutes the marker after minification, leaving an unconsumed `<!--SHOW_VARIANTS_INLINE-->` comment in the rendered HTML → **Mitigation**: the substitution is a single string replace performed inside `process_file()` before any other transformation runs. The marker is consumed before the HTML minifier sees the file.
- **[Risk]** Generated JS adds bytes to the firmware image → **Mitigation**: the JS payload is ~3 KB raw and ~1 KB minified+gzip, which is well within the 320 KB RAM budget and the 2 MB flash budget. `src/generated/control_gz.h` grows by roughly that amount; the gzip step is unchanged.
- **[Risk]** A future maintainer assumes `gen_show_variants.js` is generated once and committed forever, and doesn't notice when the JSON changes invalidate the JS payload → **Mitigation**: `compress_web.py` reads `show_variants.js` at every ESP32 build, and `gen_show_variants.py` is a `pre:` script that runs before `compress_web.py`. The build pipeline regenerates both files in order on every `pio run`. The header is committed to git (matching the existing `*_gz.h` pattern), so a stale checkout is detectable via `git diff src/generated/`.
- **[Risk]** JSON's `Solid.france` colours don't match what `loadFrenchFlag()` historically set → **Mitigation**: this change rewrites the flag buttons to read from `FLAG_PRESETS.france`, so whatever colour the JSON declares becomes the colour the button sets. There is no legacy value to preserve — `loadFrenchFlag()` is deleted and replaced.
- **[Risk]** `mandelbrotPresets` and `wavePresets` JavaScript objects in `data/control.html` may be referenced from other places (e.g. tests, debug code) that we haven't audited → **Mitigation**: `grep -rn 'mandelbrotPresets\|wavePresets' data/ src/ test/ docs/` shows references only inside `data/control.html`. Removing them from the HTML and replacing the consuming call sites (`loadMandelbrotPreset`, `loadWavePreset`) eliminates the references cleanly.

## Migration Plan

None. The change is purely:

1. One new entry in `scripts/show_variants.json` (`Solid.france`).
2. Eight header signatures edited.
3. One script extended, one new file emitted.
4. One script extended, one marker substituted.
5. ~30 lines of JS in `data/control.html` deleted and replaced with reads from the new payload.

No firmware version bump, no NVS migration, no API change. The user-visible web UI is functionally identical (same dropdown options, same flag button colours). The gallery gains one new PNG (`Solid_france.png`).

If the change must be reverted, the reverse sequence is:

1. Restore `data/control.html` from git (regenerates `control_gz.h` on next build).
2. Restore the eight `src/show/*.h` headers from git.
3. Revert `compress_web.py`, `gen_show_variants.py`.
4. Remove `Solid.france` from `scripts/show_variants.json` (also reverts the `Solid_france.png` preview).

No user data is touched.

## Open Questions

- **Should the parity test (`test_merge_default_params_matches_json_for_every_show`) explicitly assert the Mandelbrot 1-ULP tolerance?** The current test asserts byte-equality for all 12 shows including Mandelbrot, and passes — ArduinoJson 7's serialisation truncates to the same precision the JSON uses, so the bit-pattern drift doesn't surface at the JSON layer. The spec language says the Mandelbrot exception "SHALL be permitted by the runtime parity test", which is misleading because no exception is needed. A small follow-up could either tighten the test (parse the merged `JsonDocument`, inspect `Cim0`/`Cim1` floats, assert 1-ULP) or update the spec wording. Out of scope here.