# canonical-show-config Specification

## Purpose
TBD - created by archiving change canonical-show-config-file. Update Purpose after archive.
## Requirements
### Requirement: Variants manifest is the canonical source for show defaults and presets

`scripts/show_variants.json` SHALL be the canonical source for two roles, both expressed as a single JSON object keyed by show name:

1. **Factory default** — the parameter set the runtime uses when a show is requested without explicit parameters.
2. **Curated preset** — a parameter set the user can pick from a dropdown or that the gallery can preview.

Both roles SHALL be present in the file. A show that has only one interesting parameter set (its default) SHALL still declare the default explicitly.

#### Scenario: File declares both roles per show
- **WHEN** the manifest is parsed
- **THEN** every show entry has a top-level `default` object with a `params` field
- **THEN** every show entry has a `variants` array (possibly empty)

#### Scenario: A show with no curated presets
- **WHEN** a show has no interesting variants beyond its default (e.g. `ColorRun`, `Jump`)
- **THEN** the show's `variants` array is empty (`[]`)
- **THEN** the show's `default` is still present and materialised

### Requirement: Default is structurally distinct from variants

The factory default SHALL live in a top-level `default` object on the show entry, separate from the `variants` array. The default SHALL NOT be a member of `variants`. There SHALL be at most one `default` per show.

```jsonc
{
  "ShowName": {
    "description": "...",
    "iterations": 300,                  // optional, show-level render-row override
    "default": {
      "params": {<fully materialised>}
    },
    "variants": [
      { "name": "...", "label": "...", "params": {...}, "iterations": 50 },
      ...
    ]
  }
}
```

#### Scenario: Default is a sibling of variants, not an entry
- **WHEN** a consumer parses the manifest
- **THEN** `body["default"]` returns the default's `params` and any default-level overrides
- **THEN** `body["variants"]` returns the curated presets only; the default does not appear in this list

#### Scenario: No duplicate default-in-variants
- **WHEN** a variant in `variants` has `params` byte-identical to `default.params`
- **THEN** the variant is removed from `variants` and the default is the sole source for that parameter set

### Requirement: Every variant's params is complete (no implicit C++ fallback)

The `params` object of `default` and every entry in `variants` SHALL list every key the on-device factory reads for that show. A consumer reading the manifest SHALL NOT need to consult any C++ source to know what params a variant uses. The empty-object shorthand (`{}`) SHALL NOT appear on any variant's `params` in the manifest after this capability is in force.

#### Scenario: Fully materialised default for a multi-field show
- **WHEN** the manifest declares `Fire`'s default
- **THEN** `default.params` carries all six keys: `cooling`, `spread`, `ignition`, `spark_amount`, `start_offset`, `spark_range`
- **THEN** every key has the same value the on-device `ShowFactory` `|` fallback produces for `{}` today

#### Scenario: Fully materialised non-default variant
- **WHEN** the manifest declares `Fire.high`
- **THEN** its `params` carries `start_offset` and `spark_range` explicitly, even though those fields are also the C++ defaults

#### Scenario: Show with no params keeps an empty default.params
- **WHEN** a show takes no parameters at runtime (e.g. `ColorRun`, `Jump`)
- **THEN** `default.params` is `{}` and `variants` is `[]`
- **THEN** `{}` here means "the show has no parameters," not "rely on a C++ fallback"

### Requirement: Default.params for MorseCode uses "HELLO WORLD"

The MorseCode default message SHALL be the string `"HELLO WORLD"` and SHALL be declared identically in all four locations:

1. `scripts/show_variants.json` `MorseCode.default.params.message`
2. `src/show/factory/ShowFactory.cpp` factory fallback (the literal on the right-hand side of `|` for the `"message"` key in `MorseCode`'s lambda)
3. `src/show/MorseCode.h` `MorseCode` constructor default argument for the `message` parameter
4. `data/control.html` `<input type="text" id="morseMessage">` element's `value` attribute

A drift between any pair of these locations is a defect in `canonical-show-config`. The four locations form a single contract: there is one canonical default message, and every surface that names it names the same string.

#### Scenario: All four locations declare "HELLO WORLD"
- **WHEN** the manifest is parsed
- **THEN** `MorseCode.default.params["message"]` equals `"HELLO WORLD"`
- **WHEN** `src/show/factory/ShowFactory.cpp` parses `{"name":"MorseCode","params":{}}`
- **THEN** the factory fallback for the `"message"` key returns the string `"HELLO WORLD"`
- **WHEN** `src/show/MorseCode.h` `MorseCode` constructor is inspected
- **THEN** the default value of the `message` parameter equals `"HELLO WORLD"`
- **WHEN** `data/control.html` is rendered
- **THEN** the `<input type="text" id="morseMessage">` element has `value="HELLO WORLD"`

#### Scenario: Empty-params API call scrolls "HELLO WORLD"
- **WHEN** the firmware receives `POST /api/show {"name":"MorseCode","params":{}}`
- **THEN** the strip scrolls the message `"HELLO WORLD"`
- **THEN** no other parameter is overridden (the other six keys still come from their C++ `|` fallbacks, which match the JSON `default.params` exactly)

#### Scenario: User-supplied message is honoured
- **WHEN** the firmware receives `POST /api/show {"name":"MorseCode","params":{"message":"SOS"}}`
- **THEN** the strip scrolls the message `"SOS"` regardless of any of the four default locations
- **THEN** the empty-params path is unaffected by this rule

### Requirement: Schema is consumable by Python and by future C++ / JS consumers

The manifest SHALL be parseable by Python's stdlib `json.load()` with no preprocessing. It SHALL NOT contain JSON5 features, comments other than the top-of-file `_comment`, trailing commas, or non-string keys. The manifest SHALL be consumed by a Python build script that emits a `constexpr` C++ header (`src/generated/show_variants.h`) included by the firmware, and the firmware's runtime default parameters SHALL come from that header. The manifest SHALL also be consumed by a Python build script that emits a JS payload (`src/generated/show_variants.js`) inlined into `data/control.html` by `scripts/compress_web.py` before gzip compression, and the rendered HTML's preset dropdowns and flag buttons SHALL read from that payload. Neither generated artifact SHALL be tracked in git — both live under the gitignored `src/generated/` directory and are regenerated on every PlatformIO build by a `pre:` script (`scripts/gen_show_variants.py`). This matches the existing `*_gz.h` pattern: every web payload is generated, never committed. The `_comment` is the only non-data field; show entries carry only `description`, optional `iterations`, `default`, and `variants`; `default` carries only `params` and optional `iterations`; `variants[]` entries carry only `name`, `label`, `params`, and optional `iterations`.

#### Scenario: Manifest parses as stdlib JSON
- **WHEN** the file is read with `python3 -c "import json; json.load(open('scripts/show_variants.json'))"`
- **THEN** no exception is raised

#### Scenario: `_comment` is the only non-data field
- **WHEN** the manifest is parsed
- **THEN** the only top-level keys are `_comment` and the show names
- **THEN** show entries carry only `description`, optional `iterations`, `default`, and `variants`
- **THEN** `default` carries only `params` and optional `iterations`
- **THEN** `variants[]` entries carry only `name`, `label`, `params`, and optional `iterations`

#### Scenario: A Python build script emits a C++ header and a JS payload
- **WHEN** `python3 scripts/gen_show_variants.py` is run (manually or as a PlatformIO `pre:` script)
- **THEN** `src/generated/show_variants.h` is regenerated
- **THEN** the header declares a `constexpr` table of every show's `name`, `description`, `default_params_json`, and `variants[]` (each variant carrying `name`, `label`, `params_json`)
- **THEN** `src/generated/show_variants.js` is regenerated in the same run
- **THEN** the JS payload declares `const SHOW_VARIANTS_BY_SHOW = { ... }` covering every show's `default` and `variants[]` entries, and `const FLAG_PRESETS = { ... }` covering the three flag entries from `Solid.variants[]`
- **THEN** both files are valid (the header compiles in `pio run -e adafruit_qtpy_esp32s3_nopsram`; the JS parses with `node -e "require('./src/generated/show_variants.js')"` or equivalent)

#### Scenario: Firmware runtime defaults come from the generated header
- **WHEN** the firmware builds a show via `ShowFactory::createShow(name, paramsJson)`
- **THEN** the factory looks up `name` in the generated header
- **WHEN** the lookup succeeds
- **THEN** the factory parses the header's `default_params_json` into a `JsonDocument` and overlays the user's `paramsJson` on top via ArduinoJson 7's `JsonDocument::set()` deep-merge
- **WHEN** the overlay completes
- **THEN** the merged `JsonDocument` is passed to the registered factory lambda
- **WHEN** the lookup fails (show not in the header)
- **THEN** the factory passes the user's `paramsJson` directly to the lambda (existing behaviour; defensive escape hatch)

#### Scenario: Web UI consumes the JS payload from the inlined HTML
- **WHEN** the browser receives the rendered `data/control.html` (gzipped and decompressed by the firmware)
- **THEN** the page contains a `<script>` element whose body declares `SHOW_VARIANTS_BY_SHOW` and `FLAG_PRESETS`
- **WHEN** the user opens the Mandelbrot or Wave preset dropdown
- **THEN** the dropdown options are populated from `SHOW_VARIANTS_BY_SHOW.<ShowName>`
- **WHEN** the user clicks a flag button
- **THEN** the corresponding `load*Flag()` function reads from `FLAG_PRESETS.<name>`

### Requirement: Show-level iterations applies to the default

The show entry's optional `iterations` field SHALL apply to `default` and to every entry in `variants` that does not carry its own `iterations`. A per-variant `iterations` override SHALL continue to take precedence for that variant. A future C++ consumer SHALL NOT need to honour `iterations` (it is a renderer concern only); the field is preserved as-is.

#### Scenario: Mandelbrot iterations lifted to show level
- **WHEN** the manifest declares `Mandelbrot` with `"iterations": 1500` at the show body
- **THEN** the default and every variant render at 1500 rows in the gallery
- **THEN** no per-variant `iterations` override is needed

### Requirement: Adding a new show to the manifest is enough to preview it

A maintainer SHALL be able to add a new show to the gallery by adding an entry to `scripts/show_variants.json` with at least a `description` and a `default` object. No source-code change to `ShowFactory`, the simulator binary, or the firmware is required for the gallery to preview the new show. The renderer SHALL fall back to a single empty-params variant for shows not present in the manifest, preserving the existing behaviour for backwards compatibility during incremental rollout.

#### Scenario: New show appears in the gallery without source change
- **WHEN** a new entry is added to the manifest under a show name registered in `ShowFactory`
- **THEN** the next `python3 scripts/build_pages.py --all docs/show_previews` invocation produces a `{ShowName}_default.png`
- **THEN** `docs/show_previews/index.html` lists the new show alongside the existing shows

#### Scenario: Manifest-omitted show still previews
- **WHEN** `ShowFactory` registers a show that is absent from the manifest
- **THEN** the renderer produces a `{ShowName}_default.png` rendered with `--params "{}"`
- **THEN** the gallery still lists the show with a "Default" label

### Requirement: Firmware runtime default parameters equal the JSON default.params

For every show whose `default.params` is non-empty in the manifest, the firmware's runtime path `ShowFactory::createShow(name, "{}")` SHALL produce a `JsonDocument` whose serialised form is byte-equal to the JSON's `default.params` for that show. The exception is `Mandelbrot`, where ArduinoJson 7's parser rounds `-0.3616` and `-0.3156` to float bit patterns one ULP away from the literal `0xbeb923a3` and `0xbea19653` (respectively the parser emits `0xbeb923a2` and `0xbea19652`). This divergence is documented in the prior `canonical-show-config-file` change's `design.md` and SHALL be permitted by the runtime parity test. No other show SHALL exhibit any drift between the JSON default and the firmware's empty-params output.

#### Scenario: Eleven shows have byte-equal empty-params and JSON default
- **WHEN** `factory.createShow(name, "{}")` is called for `Solid`, `Fire`, `Starlight`, `Stroboscope`, `Rainbow`, `Wave`, `TheaterChase`, `MorseCode`, `Chaos`, `ColorRun`, or `Jump`
- **THEN** the merged `JsonDocument` serialises byte-equal to the show's `default.params` in the manifest

#### Scenario: Mandelbrot has documented 1-ULP drift
- **WHEN** `factory.createShow("Mandelbrot", "{}")` is called
- **THEN** the merged `JsonDocument` differs from `Mandelbrot.default.params` in the float bit patterns of `Cim0` and `Cim1` by at most 1 ULP
- **THEN** no other key in the Mandelbrot default differs from the manifest

#### Scenario: User-supplied params overlay the JSON default
- **WHEN** `factory.createShow(name, paramsJson)` is called with non-empty `paramsJson`
- **THEN** the user's keys override the JSON default's keys for any key present in both
- **WHEN** the user omits a key the JSON default declares
- **THEN** the JSON default's value for that key is preserved in the merged doc
- **THEN** the merged doc is passed to the factory lambda

#### Scenario: Touch controller Solid variants come from the JSON
- **WHEN** the touch controller's entry 0 in `SHOW_VARIANTS[]` (Solid single-colour presets) is enumerated
- **THEN** the variant list is sourced from the generated header's `ShowVariants::kShows[0].variants[]` for show name `Solid`
- **THEN** the 10 Solid single-colour entries in the JSON (`warm-white`, `pure-white`, `red`, `orange`, `yellow`, `green`, `cyan`, `sky-blue`, `blue`, `magenta`) are the touch controller's Solid variants
- **THEN** the touch controller's `src/TouchController.cpp` no longer declares a hand-coded `SOLID_VARIANTS[]` array for those 10 entries
- **THEN** the touch controller's other entries (Solid color-ranges, Solid two-color-blends, ColorRun, Jump, Rainbow, Wave, Fire, Starlight, TheaterChase, MorseCode) remain hand-coded as touch-UX-curated presets

#### Scenario: Native parity test asserts byte-equality
- **WHEN** `pio test -e native` runs the parity test
- **THEN** for every show in `ShowVariants::kShows` except `Mandelbrot`, `factory.createShow(name, "{}")` produces a serialised `JsonDocument` byte-equal to `kShows[i].default_params_json`
- **WHEN** the show is `Mandelbrot`
- **THEN** the test asserts the documented ULP tolerance on `Cim0` and `Cim1` and passes
- **THEN** the test fails on any other divergence between the JSON default and the firmware output

### Requirement: Constructors in src/show/*.h SHALL NOT carry default arguments for parameters the JSON declares defaults for

For every show class in `src/show/*.h`, every constructor parameter that the JSON's `default.params` declares SHALL be a required parameter in C++ — no `= value` default argument. The C++ header becomes a pure signature; the values live in `scripts/show_variants.json` and are read at runtime via the generated header. A drift between the constructor signature's defaults (now nonexistent) and the JSON's defaults is therefore structurally impossible: there are no defaults in C++ to drift from.

The change applies to every parameterised constructor: `MorseCode`, `Starlight`, `Wave`, `Fire`, `Mandelbrot`, `TheaterChase`, `Stroboscope`, `Rainbow`. Parameterless constructors (`ColorRun`, `Jump`) are unaffected.

The `|` fallback chain in `src/show/factory/ShowFactory.cpp` factory lambdas is unchanged. Those fallbacks remain as defense-in-depth for any key the JSON does not declare; this requirement governs only the C++ header signatures.

#### Scenario: Every parameterised constructor has zero default arguments
- **WHEN** `src/show/*.h` is searched with `grep -nE '=\s*[0-9]+(\.[0-9]+)?[fL]?\s*[,)]' src/show/*.h | grep -v '//'`
- **THEN** the only matches are in comment lines or unrelated to constructor parameter defaults
- **WHEN** the headers are inspected
- **THEN** `MorseCode`'s constructor's `message`, `speed`, `dot_length`, `dash_length`, `symbol_space`, `letter_space`, `word_space` parameters have no default arguments
- **THEN** `Starlight`'s constructor's `probability`, `length_ms`, `fade_ms`, `r`, `g`, `b` parameters have no default arguments
- **THEN** `Wave`'s constructor's `decay_rate`, `brightness_frequency`, `mode` parameters have no default arguments
- **THEN** `Fire`'s constructor's `cooling`, `spread`, `ignition`, `spark_amount`, `spark_amount`, `start_offset`, `spark_range` parameters have no default arguments
- **THEN** `Mandelbrot`'s constructor's `cReMin`, `cImMin`, `cImMax`, `scale`, `max_iterations`, `color_scale` parameters have no default arguments
- **THEN** `TheaterChase`'s constructor's `num_steps_per_cycle` parameter has no default argument
- **THEN** `Stroboscope`'s constructor's `r`, `g`, `b`, `on_cycles`, `off_cycles` parameters have no default arguments
- **THEN** `Rainbow`'s constructor's `time_step`, `pixel_step` parameters have no default arguments

#### Scenario: Every caller supplies explicit values for every parameter
- **WHEN** the codebase is searched with `grep -r 'new\s\+\(MorseCode\|Starlight\|Wave\|Fire\|Mandelbrot\|TheaterChase\|Stroboscope\|Rainbow\|Solid\|ColorRanges\)' src/ test/`
- **THEN** every match either lives inside `src/show/factory/ShowFactory.cpp` (or its companion `.h`) or has been updated in this change to supply explicit values for every parameter — no call relies on a default argument
- **WHEN** `pio run -e adafruit_qtpy_esp32s3_nopsram` builds
- **THEN** the build succeeds with no "missing default argument" errors at any call site

#### Scenario: Constructor signatures match every call site
- **WHEN** every construction site (production code and tests) is inspected
- **THEN** every `std::make_unique<Show::Foo>(...)`, `new Show::Foo(...)`, and `Show::Foo x(...)` call supplies an explicit value for every parameter
- **THEN** no call relies on a default argument that no longer exists

#### Scenario: Default-params source is JSON only
- **WHEN** `src/show/*.h` is searched for any literal value that appears in `scripts/show_variants.json` (e.g. `"HELLO WORLD"`, `0.1f`, `21`, `50`)
- **THEN** matches occur only inside function bodies or comments — never as a constructor parameter default

### Requirement: data/control.html preset dropdowns and flag buttons SHALL source values from the JSON via the generated JS payload

The `data/control.html` web interface contains three UI affordances that previously declared default values independently of the JSON: the `mandelbrotPresets` JavaScript object (used by the Mandelbrot preset dropdown), the `wavePresets` JavaScript object (used by the Wave preset dropdown), and the `loadUkraineFlag()`, `loadItalianFlag()`, `loadFrenchFlag()` functions (used by the three flag buttons). All three SHALL read their values from a JS payload generated from `scripts/show_variants.json` by `scripts/gen_show_variants.py`. The JS payload SHALL be inlined into the rendered HTML by `scripts/compress_web.py` before gzip compression, and SHALL be reachable as `SHOW_VARIANTS_BY_SHOW.<ShowName>` for the per-show preset dictionaries and `FLAG_PRESETS.<name>` for the three flag entries.

The three flag entries SHALL be declared in `scripts/show_variants.json` as `Solid.variants[ukraine]`, `Solid.variants[italy]`, and `Solid.variants[france]`. The `france` entry is added by this change; it did not exist before.

#### Scenario: Mandelbrot preset dropdown reads from SHOW_VARIANTS_BY_SHOW.Mandelbrot
- **WHEN** the user opens the Mandelbrot preset dropdown in `data/control.html`
- **THEN** the dropdown lists `default`, `deep-zoom`, `wide-overview`, `mid-zoom`, `mid-zoom-2`, `intermediate-zoom`, `spiral`
- **THEN** every option's value matches the corresponding entry in `scripts/show_variants.json` `Mandelbrot.default.params` or `Mandelbrot.variants[].params`
- **WHEN** the user selects an option
- **THEN** the Mandelbrot parameter form fields are populated from `SHOW_VARIANTS_BY_SHOW.Mandelbrot[<option>]` (which `compress_web.py` inlines into the page)
- **THEN** no `mandelbrotPresets` JavaScript literal exists in `data/control.html` after this capability is in force

#### Scenario: Wave preset dropdown reads from SHOW_VARIANTS_BY_SHOW.Wave
- **WHEN** the user opens the Wave preset dropdown in `data/control.html`
- **THEN** the dropdown lists `default`, `tight`, `calm`
- **THEN** every option's value matches the corresponding entry in `scripts/show_variants.json` `Wave.default.params` or `Wave.variants[].params`
- **WHEN** the user selects an option
- **THEN** the Wave parameter form fields are populated from `SHOW_VARIANTS_BY_SHOW.Wave[<option>]`
- **THEN** no `wavePresets` JavaScript literal exists in `data/control.html` after this capability is in force

#### Scenario: Flag buttons read from FLAG_PRESETS
- **WHEN** the user clicks the Ukraine, Italy, or France flag button in `data/control.html`
- **THEN** `loadUkraineFlag()`, `loadItalianFlag()`, or `loadFrenchFlag()` reads the corresponding entry from `FLAG_PRESETS`
- **THEN** the Solid colour form fields are populated from `FLAG_PRESETS.ukraine`, `FLAG_PRESETS.italy`, or `FLAG_PRESETS.france`
- **THEN** `FLAG_PRESETS.france` exists because `scripts/show_variants.json` `Solid.variants[]` declares a `france` entry
- **THEN** no hand-coded colour literals exist in the three `load*Flag()` functions after this capability is in force

#### Scenario: France variant is in Solid.variants[]
- **WHEN** `scripts/show_variants.json` is parsed
- **THEN** `Solid.variants[]` contains an entry with `name: "france"` and a `label` describing the French flag
- **THEN** the `france` entry's `params` declares `colors` as a 3-element array (blue, white, red) and `gradient: false`

#### Scenario: gen_show_variants.py emits a JS payload
- **WHEN** `python3 scripts/gen_show_variants.py` is run (manually or as a PlatformIO `pre:` script)
- **THEN** `src/generated/show_variants.js` is regenerated
- **THEN** the file declares `const SHOW_VARIANTS_BY_SHOW = { ... }` and `const FLAG_PRESETS = { ... }`
- **THEN** `SHOW_VARIANTS_BY_SHOW` covers every show in the JSON, with `default` and every entry in `variants[]` as keys
- **THEN** `FLAG_PRESETS` covers `ukraine`, `italy`, and `france` from `Solid.variants[]`
- **THEN** every value is the JSON's `params` object (after `normalize_for_arduinojson`) serialised to JS

#### Scenario: compress_web.py inlines the JS payload before minification
- **WHEN** `scripts/compress_web.py` processes `data/control.html`
- **THEN** the file's `<!--SHOW_VARIANTS_INLINE-->` marker is replaced with a `<script>` element containing the contents of `src/generated/show_variants.js`
- **THEN** the substitution runs before `minify_html()` (so the marker is consumed before HTML comment removal)
- **THEN** the rendered HTML (after minification+gzip) carries the inlined JS payload
- **WHEN** the firmware serves the page
- **THEN** the browser receives a single HTML payload that contains both the page structure and the `SHOW_VARIANTS_BY_SHOW` / `FLAG_PRESETS` objects

