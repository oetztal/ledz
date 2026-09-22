## MODIFIED Requirements

### Requirement: Schema is consumable by Python and by future C++ / JS consumers

The manifest SHALL be parseable by Python's stdlib `json.load()` with no preprocessing. It SHALL NOT contain JSON5 features, comments other than the top-of-file `_comment`, trailing commas, or non-string keys. The manifest SHALL be consumed by a Python build script that emits a `constexpr` C++ header (`src/generated/show_variants.h`) included by the firmware, and the firmware's runtime default parameters SHALL come from that header. The header SHALL be tracked in git and regenerated on every PlatformIO build by a `pre:` script. The `_comment` is the only non-data field; show entries carry only `description`, optional `iterations`, `default`, and `variants`; `default` carries only `params` and optional `iterations`; `variants[]` entries carry only `name`, `label`, `params`, and optional `iterations`.

#### Scenario: Manifest parses as stdlib JSON
- **WHEN** the file is read with `python3 -c "import json; json.load(open('scripts/show_variants.json'))"`
- **THEN** no exception is raised

#### Scenario: `_comment` is the only non-data field
- **WHEN** the manifest is parsed
- **THEN** the only top-level keys are `_comment` and the show names
- **THEN** show entries carry only `description`, optional `iterations`, `default`, and `variants`
- **THEN** `default` carries only `params` and optional `iterations`
- **THEN** `variants[]` entries carry only `name`, `label`, `params`, and optional `iterations`

#### Scenario: A Python build script emits a C++ header
- **WHEN** `python3 scripts/gen_show_variants.py` is run (manually or as a PlatformIO `pre:` script)
- **THEN** `src/generated/show_variants.h` is regenerated
- **THEN** the header declares a `constexpr` table of every show's `name`, `description`, `default_params_json`, and `variants[]` (each variant carrying `name`, `label`, `params_json`)
- **THEN** the header is valid C++17 and compiles in `pio run -e adafruit_qtpy_esp32s3_nopsram`

#### Scenario: Firmware runtime defaults come from the generated header
- **WHEN** the firmware builds a show via `ShowFactory::createShow(name, paramsJson)`
- **THEN** the factory looks up `name` in the generated header
- **WHEN** the lookup succeeds
- **THEN** the factory parses the header's `default_params_json` into a `JsonDocument` and overlays the user's `paramsJson` on top via ArduinoJson 7's `JsonDocument::set()` deep-merge
- **WHEN** the overlay completes
- **THEN** the merged `JsonDocument` is passed to the registered factory lambda
- **WHEN** the lookup fails (show not in the header)
- **THEN** the factory passes the user's `paramsJson` directly to the lambda (existing behaviour; defensive escape hatch)

## ADDED Requirements

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
