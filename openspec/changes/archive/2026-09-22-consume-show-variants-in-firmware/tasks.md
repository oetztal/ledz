## 1. Add the 10 Solid single-colour variants to `scripts/show_variants.json`

- [x] 1.1 In `scripts/show_variants.json`, append 10 new entries to `Solid.variants[]` (after `ukraine`, `italy`, `rainbow`) with names `warm-white`, `pure-white`, `red`, `orange`, `yellow`, `green`, `cyan`, `sky-blue`, `blue`, `magenta`. Each carries a `label` and a `params` object with the single `[[r,g,b]]` colour matching the touch controller's hand-coded `SOLID_VARIANTS[]` entries at `src/TouchController.cpp:16-27`. The final `Solid.variants[]` has 13 entries: `default` (synthesised), `ukraine`, `italy`, `rainbow`, `warm-white`, `pure-white`, `red`, `orange`, `yellow`, `green`, `cyan`, `sky-blue`, `blue`, `magenta`.

## 2. Build script: `scripts/gen_show_variants.py`

- [x] 2.1 Create `scripts/gen_show_variants.py`. It reads `scripts/show_variants.json`, validates it parses as stdlib JSON, and emits `src/generated/show_variants.h` containing a `namespace ShowVariants` with `constexpr` `struct Variant { const char* name; const char* label; const char* params_json; }`, `struct ShowEntry { const char* name; const char* description; const char* default_params_json; const Variant* variants; size_t num_variants; }`, and `constexpr ShowEntry kShows[kNumShows]` plus per-show `constexpr Variant k<Name>Variants[]` arrays. The emitter does two passes: collect all variants first, then emit the show entries pointing to them. The script raises with a clear error if the JSON is malformed or if a referenced `default`/`variant` is missing required keys.
- [x] 2.2 Add `const char* findDefaultParamsJson(const char* name)` (linear scan over `kShows`, returns the matching entry's `default_params_json` or `nullptr`) and `const Variant* findVariants(const char* name, size_t* out_num_variants)` to the generated header.
- [x] 2.3 Run the script standalone (`python3 scripts/gen_show_variants.py`) and inspect `src/generated/show_variants.h`. Confirm every registered show (`Solid`, `Fire`, `Starlight`, `Stroboscope`, `ColorRun`, `Jump`, `Rainbow`, `Wave`, `TheaterChase`, `MorseCode`, `Chaos`, `Mandelbrot`) appears in `kShows` with the correct default and variant counts.
- [x] 2.4 Register the script as a PlatformIO `pre:` hook in `platformio.ini:64-66` (the ESP32 environment's `extra_scripts`) and in `platformio.ini:42` (the native simulator environment) and `platformio.ini:23-25` (the native test environment) so every build regenerates the header.

## 3. ShowFactory uses the generated header

- [x] 3.1 In `src/show/factory/ShowFactory.cpp`, `#include "generated/show_variants.h"` at the top of the file.
- [x] 3.2 Rewrite `ShowFactory::createShow(const std::string &name, const std::string &paramsJson)` (currently `src/show/factory/ShowFactory.cpp:150-170`) so it: (a) looks up `name` in `ShowVariants::kShows` via `ShowVariants::findDefaultParamsJson`; (b) if found and the default is non-empty, parses the default into a local `JsonDocument` named `merged`; (c) parses `paramsJson` into a local `JsonDocument` named `user`; (d) if the user parse succeeded, calls `merged.set(user)` to overlay user keys; (e) passes `merged` (or `user` if no default exists) to the registered factory lambda. The `|` fallback chain in the factory lambdas is unchanged.
- [x] 3.3 Confirm `pio run -e adafruit_qtpy_esp32s3_nopsram` builds cleanly with the new ShowFactory path.

## 4. TouchController uses the generated header for Solid variants

- [x] 4.1 In `src/TouchController.cpp`, `#include "generated/show_variants.h"`.
- [x] 4.2 Delete the hand-coded `SOLID_VARIANTS[]` array at `src/TouchController.cpp:16-27`.
- [x] 4.3 In `src/TouchController.cpp`, declare a new `static const char* const SOLID_VARIANTS_FROM_HEADER[]` populated from `ShowVariants::findVariants("Solid", &n)` (the JSON's `Solid.variants[]`). The 13 entries (default-synthesised not included since the JSON doesn't list it as a variant; only the named variants 10 entries are returned) match the JSON's `Solid.variants[]` order.
- [x] 4.4 Update `src/TouchController.cpp:64` `SHOW_VARIANTS[]` so entry 0 is `{"Solid", SOLID_VARIANTS_FROM_HEADER, <count>}`. Rename `COLORRANGES_VARIANTS` to `TOUCH_ONLY_COLORRANGES_VARIANTS`, `TWOCOLORBLEND_VARIANTS` to `TOUCH_ONLY_TWOCOLORBLEND_VARIANTS`, and similarly for `COLORRUN_VARIANTS`, `JUMP_VARIANTS`, `RAINBOW_VARIANTS`, `WAVE_VARIANTS`, `FIRE_VARIANTS`, `STARLIGHT_VARIANTS`, `THEATERCHASE_VARIANTS`, `MORSECODE_VARIANTS` to make the boundary explicit.
- [x] 4.5 Confirm `pio run -e adafruit_qtpy_esp32s3_nopsram` and `pio run -e native_show_sim` build cleanly with the new touch controller wiring.

## 5. Native parity test

- [x] 5.1 Create `test/test_show_factory/test_parity_with_json.cpp` (or add to the existing `test/test_show_factory/test_show_factory.cpp` if that file exists). The test iterates `ShowVariants::kShows`, calls `factory.createShow(name, "{}")` for each, serialises the result via ArduinoJson's `serializeJson(doc, buffer)`, and asserts byte-equality with the header's `default_params_json`.
- [x] 5.2 Add a `Mandelbrot`-specific branch that asserts the documented 1-ULP tolerance on `Cim0` and `Cim1` (parser emits `0xbeb923a2` vs literal `0xbeb923a3` for `-0.3616`; same for `-0.3156`) and passes.
- [x] 5.3 Confirm `pio test -e native` runs the new parity test and that all 134+ existing tests still pass.

## 6. Regenerate gallery PNGs

- [x] 6.1 Run `python3 scripts/build_pages.py --all docs/show_previews --landing --seed 42` and confirm the script exits zero.
- [x] 6.2 Confirm 10 new `Solid_*<colour>.png` files exist under `docs/show_previews/` (one per single-colour variant), alongside the existing `Solid_default.png`, `Solid_ukraine.png`, `Solid_italy.png`, `Solid_rainbow.png`.
- [x] 6.3 Spot-check `docs/show_previews/index.html` to confirm the 10 new Solid entries appear in the gallery in variant order.

## 7. Verify

- [x] 7.1 Run `python3 -c "import json; json.load(open('scripts/show_variants.json'))"` to confirm the JSON still parses.
- [x] 7.2 Run `pio run -e adafruit_qtpy_esp32s3_nopsram` and confirm the firmware builds cleanly. The generated header is regenerated as part of this build via the new `pre:` hook.
- [x] 7.3 Run `pio test -e native` and confirm all existing tests pass plus the new parity test passes.
- [x] 7.4 Run `pio run -e native_show_sim` and confirm the simulator binary still builds.
- [x] 7.5 Verify the Solid single-colour variants are byte-equal between the touch controller's `SOLID_VARIANTS_FROM_HEADER` and the JSON's `Solid.variants[]` (grep + diff).

## 8. Archive

- [x] 8.1 After all tasks above are complete and the firmware builds cleanly, archive the change via `openspec archive consume-show-variants-in-firmware`. The archive step will merge the delta spec into `openspec/specs/canonical-show-config/spec.md`, replacing the existing `Schema is consumable by Python and by future C++ / JS consumers` requirement with the new present-tense version, and adding the new `Firmware runtime default parameters equal the JSON default.params` requirement.
