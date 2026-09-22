## 1. Restructure `scripts/show_variants.json`

For each of the twelve shows, lift the variant that was doing the role of "factory default" out of the `variants` array into a new top-level `default` object, and materialise its `params`.

- [x] 1.1 `Solid`: move the existing `white` variant's `params` into `default.params`. Add `"gradient": false` (currently implicit). Remove the `white` entry from `variants`. Final `variants`: `[ukraine, italy, rainbow]`. Final `default`: `{"params": {"colors": [[255, 250, 230]], "gradient": false}}`.
- [x] 1.2 `Fire`: move the existing `default` variant into `default.params`. Materialise the six fields. Remove the `default` entry from `variants`. Final `variants`: `[high]`. Final `default`: `{"params": {"cooling": 0.1, "spread": 10.0, "ignition": 0.5, "spark_amount": 0.5, "start_offset": 5, "spark_range": 5}}`.
- [x] 1.3 `Starlight`: move the existing `warm` variant into `default.params`. Materialise the six fields. Remove the `warm` entry from `variants`. Final `variants`: `[cool]`. Final `default`: `{"params": {"probability": 0.1, "length": 5000, "fade": 1000, "r": 255, "g": 180, "b": 50}}`.
- [x] 1.4 `Stroboscope`: move the existing `white-fast` variant into `default.params`. Materialise the five fields. Remove the `white-fast` entry from `variants`. Final `variants`: `[red-slow]`. Final `default`: `{"params": {"r": 255, "g": 255, "b": 255, "on_cycles": 1, "off_cycles": 10}}`.
- [x] 1.5 `ColorRun`: move the existing `default` variant (empty `params`) into `default.params`. Final `variants`: `[]`. Final `default`: `{"params": {}}`.
- [x] 1.6 `Jump`: same shape as `ColorRun`. Final `default`: `{"params": {}}`. Final `variants`: `[]`.
- [x] 1.7 `Rainbow`: move the existing `scroll` variant into `default.params`. Materialise the two fields. Remove the `scroll` entry from `variants`. Final `variants`: `[timefreeze, compressed, spacefreeze]`. Final `default`: `{"params": {"time_step": 1.0, "pixel_step": 1.0}}`.
- [x] 1.8 `Wave`: move the existing `default` variant into `default.params` (no materialisation needed — all three fields already present). Final `variants`: `[tight, calm]`. Final `default`: `{"params": {"mode": "bounce", "decay_rate": 2.0, "brightness_frequency": 0.1}}`.
- [x] 1.9 `TheaterChase`: move the existing `default` variant into `default.params`. Materialise the one field. Final `variants`: `[tight]`. Final `default`: `{"params": {"num_steps_per_cycle": 21}}`.
- [x] 1.10 `MorseCode`: move the existing `hello` variant into `default.params`. Materialise the seven fields, declaring `"message": "HELLO WORLD"` as the canonical default. Final `variants`: `[sos]`. Final `default`: `{"params": {"message": "HELLO WORLD", "speed": 0.5, "dot_length": 2, "dash_length": 4, "symbol_space": 2, "letter_space": 3, "word_space": 5}}`.
- [x] 1.11 `Chaos`: move the existing `bifurcation` variant into `default.params`. Materialise the three fields. Final `variants`: `[chaotic]`. Final `default`: `{"params": {"Rmin": 2.95, "Rmax": 4.0, "Rdelta": 0.0002}}`.
- [x] 1.12 `Mandelbrot`: move the existing `default` variant into `default.params`. Materialise the six fields. Lift `"iterations": 1500` from every per-variant entry to the show body and remove the per-variant overrides. Final `variants`: `[deep-zoom, wide-overview, mid-zoom, mid-zoom-2, intermediate-zoom, spiral]`. Final `default`: `{"params": {"Cre0": -1.05, "Cim0": -0.3616, "Cim1": -0.3156, "scale": 5, "max_iterations": 50, "color_scale": 10}}`.

## 2. Materialise non-default variants

For every remaining variant entry in `variants[]`, ensure `params` carries every key the C++ factory reads.

- [x] 2.1 `Fire.high`: add `"start_offset": 5, "spark_range": 5` (the two fields currently implicit via C++ `|`).
- [x] 2.2 `Starlight.cool`: add `"probability": 0.1` (the one field currently implicit).
- [x] 2.3 `Wave.tight` and `Wave.calm`: add `"mode": "bounce"` to each.
- [x] 2.4 `MorseCode.sos`: add `"speed": 0.5, "dot_length": 2, "dash_length": 4, "symbol_space": 2, "letter_space": 3, "word_space": 5` (the six fields currently implicit).
- [x] 2.5 Confirm every other variant's `params` already carries every key the C++ factory reads (no edits required, but verify: `Solid` variants, `Stroboscope.red-slow`, `Rainbow` variants, `TheaterChase.tight`, `Chaos.chaotic`, all `Mandelbrot` variants).

## 3. Update the file's `_comment`

- [x] 3.1 Rewrite the top-of-file `_comment` to describe the two roles (canonical factory default + curated presets), explain that every variant's `params` is now complete, name the `default` field, and point to `scripts/build_pages.py` as the renderer that consumes it. Length stays roughly the same.

## 4. Update `scripts/build_pages.py`

- [x] 4.1 In `load_variants()`, after reading each show's body, also read `body["default"]` (when present). Return both `defaults` and `variants` per show, with `defaults` carrying `params` and `iterations` and the synthetic `name="default"` / `label="Factory default"`.
- [x] 4.2 In `_resolve_variants()`, prepend the synthetic default variant to the variant list before the existing `variants[]` entries. The function's return tuple stays the same shape (`(list[Variant], str)`); callers see the default as the first element.
- [x] 4.3 Confirm `_load_descriptions()`, `_write_gallery_index()`, and `render_all()` are unchanged — they iterate the variant list and don't care which entry is the synthetic default.
- [x] 4.4 Confirm `--list` mode is unaffected (it does not touch the variants file).

## 5. Verify

- [x] 5.1 Validate the JSON with `python3 -c "import json; json.load(open('scripts/show_variants.json'))"` (or any JSON validator).
- [x] 5.2 Run `pio run -e native_show_sim` to build the simulator binary if not already built.
- [x] 5.3 Run `python3 scripts/build_pages.py --all docs/show_previews --landing --seed 42` and confirm the script exits zero.
- [x] 5.4 For every show in the file, confirm a `{ShowName}_default.png` exists in `docs/show_previews/`.
- [x] 5.5 For the six shows whose previous default PNG was named after a descriptive variant (`Solid_white`, `Starlight_warm`, `Stroboscope_whie-fast`, `Rainbow_scroll`, `MorseCode_hello`, `Chaos_bifurcation`), confirm the new `*_default.png` is byte-for-byte identical to the previous `*_<name>.png` (the materialised `default.params` matches the variant's old `params` exactly). Use `cmp` or `python3 -c "import hashlib; ..."` against `git HEAD` of the corresponding file.
- [x] 5.6 For the remaining shows, confirm the new `*_default.png` is byte-for-byte identical to a fresh render of `--params <new default>` (the materialised params produce the same bytes the C++ `|` fallback would have produced for `{}`). Note: `Mandelbrot` differs in 1094/1,350,000 bytes (0.08% of pixels) due to ArduinoJson 7's 1-ULP parser rounding for `-0.3616` and `-0.3156` — visually imperceptible, documented in `design.md` and `proposal.md`.
- [x] 5.7 For each non-default variant, confirm its PNG is byte-for-byte identical to a fresh render with the materialised params.
- [x] 5.8 Spot-check `docs/show_previews/index.html` to confirm the "Factory default" label appears first under each show's description and the existing variant labels follow.
- [x] 5.9 Run `pio run -e adafruit_qtpy_esp32s3_nopsram` to confirm the firmware still builds cleanly (no `scripts/show_variants.json` is read by the firmware, but this guards against accidental CI breakage from a formatting change).
- [x] 5.10 Run `pio test -e native` and confirm existing tests still pass.
