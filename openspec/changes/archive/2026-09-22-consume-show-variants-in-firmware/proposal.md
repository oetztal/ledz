## Why

`scripts/show_variants.json` is the canonical description of every show's default parameters and curated presets, but the firmware's runtime defaults still live as `|` fallbacks in `src/show/factory/ShowFactory.cpp` and constructor default arguments in `src/show/*.h`. The drift class is real: the prior `converge-morse-default` change fixed one string (the headline example) by hand-syncing four locations, and the `TouchController` carries 16 hand-coded preset strings that have no representation in the JSON at all. Every future edit to a show's default params risks recreating the drift the canonical-show-config capability was created to eliminate. This change makes the JSON the runtime source for the firmware via a generated C++ header.

## What Changes

- Add a new PlatformIO `pre:` build script `scripts/gen_show_variants.py` that reads `scripts/show_variants.json` and emits a `constexpr` C++ header `src/generated/show_variants.h` containing the show names, descriptions, default-params JSON, and variant lists. The script is registered in `platformio.ini` next to the existing `compress_web.py` and `get_version.py` hooks.
- Modify `src/show/factory/ShowFactory.cpp` so that `createShow(name, paramsJson)` parses the user's params, overlays them on top of the JSON default for `name` (looked up in the generated header), and passes the merged `JsonDocument` to the factory lambda. The `|` fallback literals stay in the factory lambdas as defense-in-depth but become unreachable for any key the JSON default declares.
- Add the 10 Solid single-colour preset entries currently hard-coded in `src/TouchController.cpp:16-27` (`SOLID_VARIANTS[]`) to `scripts/show_variants.json` `Solid.variants[]` alongside the existing `ukraine`, `italy`, `rainbow` flag variants. The JSON `Solid.variants[]` becomes the canonical Solid preset set; the gallery's `docs/show_previews/` gains 10 new single-colour preview PNGs under Solid.
- Modify `src/TouchController.cpp` so the first entry in `SHOW_VARIANTS[]` (Solid single-colours) is sourced from the generated header instead of the hand-coded `SOLID_VARIANTS[]` constant. The other 10 touch entries (Solid color-ranges, Solid two-color-blends, ColorRun, Jump, Rainbow, Wave, Fire, Starlight, TheaterChase, MorseCode) stay as hand-coded `TOUCH_ONLY_*` arrays because they are touch-UX-curated, not gallery-curated. The MorseCode touch entries (`"foo bar baz"`, `"gutes neues"`) remain hand-coded per the prior decision.
- Add a native test that asserts `factory.createShow(name, "{}")` byte-equal to `serialise(default_params_from_header)` for every registered show except `Mandelbrot`, which is allowed the documented ArduinoJson 7 1-ULP rounding on `Cim0` and `Cim1`. The test lives in `test/test_show_factory/`.
- Update `openspec/specs/canonical-show-config/spec.md` to add a requirement that the firmware's runtime defaults SHALL come from the JSON via the generated header, and to amend the existing "Schema is consumable" requirement so the C++ consumer language is no longer aspirational.
- Bump `docs/show_previews/` PNG outputs as part of the gallery regeneration that already runs from `scripts/build_pages.py --all`. The 10 new Solid single-colour PNGs are new files; no existing PNGs change because the JSON `Solid.default.params` and the JSON `Solid.ukraine/italy/rainbow` params are unchanged.

**Behavioural notes:**

- `POST /api/show {"name":"MorseCode","params":{}}` continues to scroll `"HELLO WORLD"` (set by the prior `converge-morse-default` change).
- Every other empty-params request continues to produce the same strip state as before this change for 11 of 12 shows. `Mandelbrot`'s empty-params path produces a strip state that differs from the prior `{}`-via-`|` path in 1094 of 1,350,000 pixels (0.08%, visually imperceptible) due to ArduinoJson 7's 1-ULP rounding on `-0.3616` and `-0.3156`; the same divergence documented in the prior change's `design.md` is locked in here and the native test marks it as expected.
- Existing NVS-saved params are honoured unchanged: the user's payload is overlaid on the JSON default, not replacing it.
- Touch-controller cycling through Solid is unchanged: entry 0 still has 10 single-colour variants; entry 1 still has 3 color-ranges; entry 2 still has 3 two-color-blends. The only difference is where entry 0's strings come from.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `canonical-show-config`: the requirement `Schema is consumable by Python and by future C++ / JS consumers` is amended. The aspirational "future C++ consumer" language becomes present-tense: the firmware builds a `constexpr` C++ header from the JSON via a Python build script, includes it in `src/show/factory/ShowFactory.cpp`, and uses it as the runtime source for default parameters. A new requirement is added: the firmware's runtime default parameters SHALL equal the JSON's `default.params` for the corresponding show, modulo the documented `Mandelbrot` ULP exception.

## Impact

- New file: `scripts/gen_show_variants.py` (~150 lines of Python).
- New file: `src/generated/show_variants.h` (~250 lines of generated C++, tracked in git like the existing `*_gz.h` files).
- `platformio.ini:64-66`: add `pre:scripts/gen_show_variants.py` to the ESP32 environment's `extra_scripts`. The native simulator environment at `platformio.ini:42` and the native test environment at `platformio.ini:23` also gain the hook so the generated header is present in every build.
- `scripts/show_variants.json`: 10 new entries in `Solid.variants[]` (single-colour presets with names like `warm-white`, `pure-white`, `red`, etc.).
- `src/show/factory/ShowFactory.cpp`: rewrite `createShow(name, paramsJson)` to merge JSON default + user params before calling the factory lambda. `ShowFactory.cpp:0-180` is otherwise unchanged.
- `src/TouchController.cpp:16-27`: delete the hand-coded `SOLID_VARIANTS[]` array; replace `SHOW_VARIANTS[0]` with a header-sourced group. `TouchController.cpp:28-78` (other touch-only arrays and the SHOW_VARIANTS table) is unchanged.
- `test/test_show_factory/`: add `test_parity_with_json.cpp` (or extend an existing file) with the byte-equality test.
- `docs/show_previews/`: regenerate via `python3 scripts/build_pages.py --all docs/show_previews --landing --seed 42`. The 10 new Solid single-colour PNGs are new files; existing PNGs are byte-identical because `Solid.default.params` and the flag variants are unchanged.
- No new HTTP endpoint. No NVS schema change. No `ShowFactory`'s registered show list change. No constructor default argument change in `src/show/*.h` (those remain documentation-only and are cleaned up in a follow-up).
- Build cost: one extra JSON parse per `createShow` call (~100µs on ESP32). Show creation is rare (boot, `POST /api/show`, touch input) and not in the 100 Hz LED loop.
