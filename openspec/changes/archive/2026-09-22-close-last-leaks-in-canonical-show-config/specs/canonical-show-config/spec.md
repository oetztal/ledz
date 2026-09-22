## ADDED Requirements

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

## MODIFIED Requirements

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