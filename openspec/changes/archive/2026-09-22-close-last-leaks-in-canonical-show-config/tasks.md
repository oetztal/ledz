## 1. Add `france` to `Solid.variants[]`

- [x] 1.1 In `scripts/show_variants.json`, append a `france` entry to `Solid.variants[]` (after `magenta`, before the closing `]`). The entry's `name` is `"france"`, `label` is `"French flag"`, and `params` is `{"colors": [[0, 85, 164], [255, 255, 255], [239, 65, 53]], "gradient": false}`.
- [x] 1.2 Confirm `python3 -c "import json; json.load(open('scripts/show_variants.json'))"` parses the file without error.
- [x] 1.3 Confirm `Solid.variants[]` now has 14 entries (the previous 13 plus `france`) by inspecting the file or by running `python3 -c "import json; d=json.load(open('scripts/show_variants.json')); print(len(d['Solid']['variants']))"`.

## 2. Strip default arguments from `src/show/*.h` constructors

- [x] 2.1 In `src/show/MorseCode.h:42`, change `MorseCode(const std::string &message = "HELLO WORLD", float speed = 0.5f, unsigned int dot_length = 2, unsigned int dash_length = 4, unsigned int symbol_space = 2, unsigned int letter_space = 3, unsigned int word_space = 5);` to remove every `= value`. The new signature is `MorseCode(const std::string &message, float speed, unsigned int dot_length, unsigned int dash_length, unsigned int symbol_space, unsigned int letter_space, unsigned int word_space);`.
- [x] 2.2 In `src/show/Starlight.h:47`, change `Starlight(float probability = 0.01f, unsigned long length_ms = 5000, unsigned long fade_ms = 1000, uint8_t r = 255, uint8_t g = 180, uint8_t b = 50);` to remove every `= value`.
- [x] 2.3 In `src/show/Wave.h:42-43`, change `Wave(float decay_rate = 2.0f, float brightness_frequency = 0.1f, WaveMode mode = WaveMode::Bounce);` to remove every `= value`.
- [x] 2.4 In `src/show/Fire.h:53`, change the constructor's `cooling = 0.1f, spread = 10.0f, ignition = .5f, spark_amount = 0.5f, start_offset = 5, spark_range = 5` defaults to remove every `= value`. The preceding `colors` parameter (which has no default) is unaffected.
- [x] 2.5 In `src/show/Mandelbrot.h:16`, change `Mandelbrot(float cReMin, float cImMin, float cImMax, unsigned int scale = 5, unsigned int max_iterations = 50, unsigned int color_scale = 10);` to remove every `= value` from `scale`, `max_iterations`, `color_scale`.
- [x] 2.6 In `src/show/TheaterChase.h:21`, change `TheaterChase(unsigned int num_steps_per_cycle = 21);` to `TheaterChase(unsigned int num_steps_per_cycle);`.
- [x] 2.7 In `src/show/Stroboscope.h:27`, change `Stroboscope(uint8_t r = 255, uint8_t g = 255, uint8_t b = 255, unsigned int on_cycles = 1, unsigned int off_cycles = 10);` to remove every `= value`.
- [x] 2.8 In `src/show/Rainbow.h:13`, change `Rainbow(float time_step = 1.0f, float pixel_step = 1.0f);` to remove every `= value`.
- [x] 2.9 Run `grep -nE '\b\w+\s*=\s*[0-9]+(\.[0-9]+)?[fL]?\s*[,)]' src/show/*.h` and confirm the only matches are inside function bodies or comments, not in constructor parameter defaults.
- [x] 2.10 Run `pio run -e adafruit_qtpy_esp32s3_nopsram` and confirm the build succeeds. Any "missing default argument" error in `ShowFactory.cpp` or in `test/test_shows/test_shows.cpp` would surface here; fix by supplying explicit values (none required for `ShowFactory`; seven direct construction sites in `test_shows.cpp` updated to supply explicit values).
- [x] 2.11 Run `pio test -e native` and confirm all 137 existing test cases still pass (the pre-change count was 137; task 6.2 re-runs this for the final verification).

## 3. Extend `scripts/gen_show_variants.py` to emit `src/generated/show_variants.js`

- [x] 3.1 Add a second output path constant: `JS_OUTPUT_FILE = PROJECT_DIR / "src" / "generated" / "show_variants.js"`.
- [x] 3.2 After the existing C++ emission, build a JS payload string. The payload declares two top-level `const` objects:
  - `SHOW_VARIANTS_BY_SHOW`: keys are show names; values are objects whose keys are the synthetic `"default"` (from `body["default"]["params"]`) and every entry in `body["variants"]` (using each variant's `name`); values are the corresponding `params` object (after `normalize_for_arduinojson`) serialised with `json.dumps(..., separators=(",", ": "), ensure_ascii=False)` (note: a space after `:` for JS readability — the inlined JS gets minified by `compress_web.py` so the space adds zero shipped bytes).
  - `FLAG_PRESETS`: keys are `"ukraine"`, `"italy"`, `"france"`; values are the corresponding entries from `Solid.variants[]` `params` object (after `normalize_for_arduinojson`) serialised with `json.dumps(..., separators=(",", ": "), ensure_ascii=False)`.
- [x] 3.3 Write the JS payload to `JS_OUTPUT_FILE`. Wrap the two `const` declarations in a trailing semicolon and a final newline to match the existing `OUTPUT_FILE.write_text(..., encoding="utf-8")` pattern.
- [x] 3.4 Run `python3 scripts/gen_show_variants.py` and confirm both `src/generated/show_variants.h` and `src/generated/show_variants.js` are written. Confirm `src/generated/show_variants.h` is byte-identical to its pre-change content (the C++ emission path is unchanged).
- [x] 3.5 Open `src/generated/show_variants.js` and confirm:
  - The file starts with `const SHOW_VARIANTS_BY_SHOW = {` and ends with `};`.
  - The file contains `const FLAG_PRESETS = {` followed by three entries: `ukraine`, `italy`, `france`.
  - Every show registered in the JSON has an entry in `SHOW_VARIANTS_BY_SHOW` with `default` and (where present) variant keys.
  - `mandelbrotPresets` values for every entry in `SHOW_VARIANTS_BY_SHOW.Mandelbrot` match the JSON's `Mandelbrot.default.params` and `Mandelbrot.variants[].params` (e.g. `Cre0: -1.05`, `Cim0: -0.3616`).
  - `wavePresets` values for every entry in `SHOW_VARIANTS_BY_SHOW.Wave` match the JSON's `Wave.default.params` and `Wave.variants[].params` (e.g. `decay_rate: 2.0`, `brightness_frequency: 0.1`).
  - `FLAG_PRESETS.france.colors` is `[[0, 85, 164], [255, 255, 255], [239, 65, 53]]` and `FLAG_PRESETS.france.gradient` is `false`.
- [x] 3.6 Confirm `src/generated/show_variants.js` is regenerated on every PlatformIO build (it lives under the gitignored `src/generated/` directory, matching the existing `*_gz.h` pattern; not tracked in git by design).

## 4. Extend `scripts/compress_web.py` to inline `show_variants.js` into `data/control.html`

- [x] 4.1 Add a constant near the top of `compress_web.py`: `JS_PAYLOAD_PATH = GENERATED_DIR / "show_variants.js"`. Add a marker constant: `JS_INLINE_MARKER = "<!--SHOW_VARIANTS_INLINE-->"`.
- [x] 4.2 Add a helper function `inline_js_payload(html: str) -> str` that reads `JS_PAYLOAD_PATH`, builds the inline string `f"<script>\n{js_contents}\n</script>"`, and returns `html.replace(JS_INLINE_MARKER, inline_string, 1)`. If `JS_PAYLOAD_PATH` does not exist, the helper raises a clear error pointing to `scripts/gen_show_variants.py`.
- [x] 4.3 In `process_file()`, before any other transformation runs (before `minify_html()` and before `gzip.compress()`), call `content = inline_js_payload(content)` after the file is read. The substitution must happen on the raw HTML so the marker is consumed before HTML comment removal.
- [x] 4.4 In `data/control.html`, add the marker `<!--SHOW_VARIANTS_INLINE-->` at the location where the inlined `<script>` element should appear (inside the existing `<script>` block at the top of the page, after any prerequisite `const` declarations like `pendingParameterConfig` and before any function that reads from `SHOW_VARIANTS_BY_SHOW`).
- [x] 4.5 Run `python3 scripts/compress_web.py` and confirm `src/generated/control_gz.h` is regenerated. The byte size of `control_gz.h` should grow by roughly the gzipped size of `src/generated/show_variants.js`.
- [x] 4.6 Decompress the generated `control_gz.h`'s payload (e.g. with `python3 -c "import gzip; print(gzip.decompress(open('src/generated/control_gz.h').read().split(b'=')[1].split(b';')[0].encode()).decode())"`) and confirm:
  - The decompressed HTML contains the literal string `const SHOW_VARIANTS_BY_SHOW = {` and `const FLAG_PRESETS = {`.
  - The marker `<!--SHOW_VARIANTS_INLINE-->` is no longer present in the rendered HTML.
  - The HTML is still well-formed (no unclosed tags, no leftover markers).

## 5. Replace the three handle-loaded dictionaries in `data/control.html`

- [x] 5.1 Delete the `mandelbrotPresets` JavaScript object literal (currently at `data/control.html:424-432`). Replace with `const mandelbrotPresets = SHOW_VARIANTS_BY_SHOW.Mandelbrot;`.
- [x] 5.2 Delete the `wavePresets` JavaScript object literal (currently at `data/control.html:435-439`). Replace with `const wavePresets = SHOW_VARIANTS_BY_SHOW.Wave;`.
- [x] 5.3 Rewrite `loadUkraineFlag()` to set the Solid colour form fields from `FLAG_PRESETS.ukraine`. The function body is approximately:
  ```javascript
  function loadUkraineFlag() {
      const p = FLAG_PRESETS.ukraine;
      setSolidColors(p.colors);
      document.getElementById('solidGradient').checked = p.gradient;
  }
  ```
  (Adapt to the existing `setSolidColors` / form-field setter pattern used by the surrounding code; the goal is that the colour values come from `FLAG_PRESETS`, not from hand-coded literals.)
- [x] 5.4 Rewrite `loadItalianFlag()` to read from `FLAG_PRESETS.italy`, mirroring the `loadUkraineFlag()` shape.
- [x] 5.5 Rewrite `loadFrenchFlag()` to read from `FLAG_PRESETS.france`, mirroring the same shape. The `france` entry now exists in `Solid.variants[]` (added in task 1.1).
- [x] 5.6 Run `grep -nE 'mandelbrotPresets\s*=\s*\{' data/control.html` and confirm there is no longer a hand-coded object literal (only the reference `SHOW_VARIANTS_BY_SHOW.Mandelbrot` survives).
- [x] 5.7 Run `grep -nE 'wavePresets\s*=\s*\{' data/control.html` and confirm there is no longer a hand-coded object literal.
- [x] 5.8 Run `grep -nE '\[\s*0\s*,\s*87\s*,\s*183\s*\]' data/control.html` and confirm no hand-coded Ukraine colour literals remain (the value now lives only in `scripts/show_variants.json` and is reachable via `FLAG_PRESETS.ukraine`).
- [x] 5.9 Run `python3 scripts/compress_web.py` to regenerate `src/generated/control_gz.h`. The byte size of `control_gz.h` should be unchanged from task 4.5 (the deletions roughly balance the JS inlining).

## 6. Verify

- [x] 6.1 Run `pio run -e adafruit_qtpy_esp32s3_nopsram` and confirm the firmware builds. The `pre:scripts/gen_show_variants.py` and `pre:scripts/compress_web.py` hooks fire in order; both files are regenerated before compilation.
- [x] 6.2 Run `pio test -e native` and confirm all 137 existing test cases pass, including `test_merge_default_params_matches_json_for_every_show` (which asserts byte-equality between `factory.createShow(name, "{}")` and `kShows[i].default_params_json` for every show).
- [x] 6.3 Run `python3 scripts/build_pages.py --all docs/show_previews --landing --seed 42` and confirm the script exits zero.
- [x] 6.4 Confirm `docs/show_previews/Solid_france.png` exists. Confirm `docs/show_previews/index.html` lists a `Solid_france.png` thumbnail with label `French flag`. All other PNGs are byte-identical to their pre-change counterparts (no other JSON entries changed).
- [x] 6.5 Spot-check the rendered web UI by decompressing `src/generated/control_gz.h` and confirming:
  - The Mandelbrot preset `<select>` has options `default`, `deep-zoom`, `wide-overview`, `mid-zoom`, `mid-zoom-2`, `intermediate-zoom`, `spiral`.
  - The Wave preset `<select>` has options `default`, `tight`, `calm`.
  - The three flag buttons are still present and their `onclick` attributes still reference `loadUkraineFlag`, `loadItalianFlag`, `loadFrenchFlag`.
  - The `mandelbrotPresets`, `wavePresets` JS variables are present and their values come from `SHOW_VARIANTS_BY_SHOW`.
  - The `FLAG_PRESETS` JS variable is present and has three keys.
- [x] 6.6 Run `git status` and confirm the following files are staged or modified:
  - `scripts/show_variants.json` (1 line for `Solid.france`)
  - `src/show/MorseCode.h`, `Starlight.h`, `Wave.h`, `Fire.h`, `Mandelbrot.h`, `TheaterChase.h`, `Stroboscope.h`, `Rainbow.h` (8 files, default-arg removal)
  - `scripts/gen_show_variants.py` (extended to emit `show_variants.js`)
  - `scripts/compress_web.py` (extended with `inline_js_payload`)
  - `data/control.html` (markers added, three objects/functions rewritten)
  - `test/test_shows/test_shows.cpp` (direct show construction sites updated to supply explicit values for every parameter — the spec's "no `= value` defaults" rule applies to every caller, not only the factory)
  - *Not in `git status` (gitignored, regenerated on every build):* `src/generated/show_variants.{h,js}`, `src/generated/control_gz.h`.

## 7. Archive

- [x] 7.1 After all tasks above are complete and the firmware builds cleanly with `pio test -e native` green, archive the change via `openspec archive close-last-leaks-in-canonical-show-config`. The archive step will merge the delta spec from `openspec/changes/close-last-leaks-in-canonical-show-config/specs/canonical-show-config/spec.md` into `openspec/specs/canonical-show-config/spec.md`, adding the two new requirements and amending the existing `Schema is consumable by Python and by future C++ / JS consumers` requirement.