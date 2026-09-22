## MODIFIED Requirements

### Requirement: Schema is consumable by Python and by future C++ / JS consumers

The manifest SHALL be parseable by Python's stdlib `json.load()` with no preprocessing. It SHALL NOT contain JSON5 features, comments other than the top-of-file `_comment`, trailing commas, or non-string keys. The manifest SHALL be consumed by a Python build script that emits a `constexpr` C++ header (`src/generated/show_variants.h`) included by the firmware, and the firmware's runtime default parameters SHALL come from that header. The manifest SHALL also be consumed by a Python build script that emits a JS payload (`src/generated/show_variants.js`) inlined into `data/control.html` by `scripts/compress_web.py` before gzip compression, and the rendered HTML's generic preset selector and flag buttons SHALL read from that payload. Neither generated artifact SHALL be tracked in git — both live under the gitignored `src/generated/` directory and are regenerated on every PlatformIO build by a `pre:` script (`scripts/gen_show_variants.py`). This matches the existing `*_gz.h` pattern: every web payload is generated, never committed. The `_comment` is the only non-data field; show entries carry only `description`, optional `iterations`, `default`, and `variants`; `default` carries only `params` and optional `iterations`; `variants[]` entries carry only `name`, `label`, `params`, and optional `iterations`.

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
- **THEN** the JS payload declares `const SHOW_VARIANTS_BY_SHOW = { ... }` covering every show's `default` and `variants[]` entries, where each entry is an object `{ label, params }` carrying the manifest's `label` (the synthetic `default` entry carrying the label `"Default"`) and its `params` object, and `const FLAG_PRESETS = { ... }` covering the three flag entries from `Solid.variants[]` as plain `params` objects
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
- **WHEN** the user selects a show that has curated variants
- **THEN** the generic preset selector's options are populated from `SHOW_VARIANTS_BY_SHOW.<ShowName>`
- **WHEN** the user clicks a flag button
- **THEN** the corresponding `load*Flag()` function reads from `FLAG_PRESETS.<name>`

### Requirement: data/control.html preset dropdowns and flag buttons SHALL source values from the JSON via the generated JS payload

The `data/control.html` web interface sources its preset values from a JS payload generated from `scripts/show_variants.json` by `scripts/gen_show_variants.py`. The JS payload SHALL be inlined into the rendered HTML by `scripts/compress_web.py` before gzip compression, and SHALL be reachable as `SHOW_VARIANTS_BY_SHOW.<ShowName>` for the per-show preset dictionaries and `FLAG_PRESETS.<name>` for the three flag entries.

The per-show preset dictionaries SHALL carry `{ label, params }` per entry, so the generic preset selector renders the manifest's label and applies the entry's params. The three flag entries SHALL be declared in `scripts/show_variants.json` as `Solid.variants[ukraine]`, `Solid.variants[italy]`, and `Solid.variants[france]`, and the flag buttons SHALL read their params from `FLAG_PRESETS`. The previous hand-coded `mandelbrotPresets` and `wavePresets` JavaScript objects, the `mandelbrotPreset` and `wavePreset` `<select>` elements, and the `loadMandelbrotPreset()` / `loadWavePreset()` functions SHALL be removed; the generic preset selector SHALL serve those shows instead.

#### Scenario: Generic preset selector reads from SHOW_VARIANTS_BY_SHOW
- **WHEN** the user selects the `Mandelbrot` or `Wave` show, or any other show with curated variants
- **THEN** the selector's options are populated from `SHOW_VARIANTS_BY_SHOW.<ShowName>` (which `compress_web.py` inlines into the page)
- **THEN** every option's label matches the corresponding entry's `label` in `scripts/show_variants.json`
- **THEN** selecting an option applies its `params` object via `POST /api/show`
- **THEN** no `mandelbrotPreset` or `wavePreset` `<select>` element exists in `data/control.html` after this capability is in force
- **THEN** no `loadMandelbrotPreset` or `loadWavePreset` function exists in `data/control.html` after this capability is in force

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
- **THEN** every `SHOW_VARIANTS_BY_SHOW` entry is an object carrying `label` and `params`, the `default` entry's `label` being `"Default"`
- **THEN** `FLAG_PRESETS` covers `ukraine`, `italy`, and `france` from `Solid.variants[]`
- **THEN** every flag value is the JSON's `params` object (after `normalize_for_arduinojson`) serialised to JS

#### Scenario: compress_web.py inlines the JS payload before minification
- **WHEN** `scripts/compress_web.py` processes `data/control.html`
- **THEN** the file's `<!--SHOW_VARIANTS_INLINE-->` marker is replaced with a `<script>` element containing the contents of `src/generated/show_variants.js`
- **THEN** the substitution runs before `minify_html()` (so the marker is consumed before HTML comment removal)
- **THEN** the rendered HTML (after minification+gzip) carries the inlined JS payload
- **WHEN** the firmware serves the page
- **THEN** the browser receives a single HTML payload that contains both the page structure and the `SHOW_VARIANTS_BY_SHOW` / `FLAG_PRESETS` objects
