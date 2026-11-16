# Tasks

## 1. Feedback engine

- [x] 1.1 Add `src/support/Feedback.{h,cpp}` with `FeedbackTransform` and `Feedback` (`begin`, `add`, `addPixel`, `render`, two swapped float RGB buffers, lazy resize) per design.md. Verify `pio run -e native` and `pio run -e adafruit_qtpy_esp32s3_nopsram` build.
- [x] 1.2 Add native suite `test/test_feedback` that covers every `feedback-rendering` scenario: persistence, geometric decay, zero decay, fade to true black within 60 iterations, saturation, zoom outward and zoom identity, scroll wrap, zero warp, mirror symmetry, black out-of-range samples, buffer size `3*N` and strip-length change. Verify `pio test -e native -f test_feedback` passes.

## 2. Tunnel show

- [x] 2.1 Add `src/show/Tunnel.{h,cpp}` (constructor with no default arguments, clamps `zoom`/`decay`/`hue_speed`/`warp`, centre emission of `wheel(hue)` through `Support::Feedback`, symmetric centre for even N). Verify it builds natively.
- [x] 2.2 Add a `Tunnel` entry to `scripts/show_variants.json` (default `{zoom:0.97, decay:0.93, hue_speed:2.5, warp:0.0, push:1.0, pulse:20}`, variants `hyperspace` and `liquid`), register it in `src/show/factory/ShowFactory.cpp` with the `|` fallbacks, and include it where shows are included. Verify the generated `src/generated/show_variants.h` contains Tunnel and that the canonical-default test in `test_show_factory` covers it.
- [x] 2.3 Add Tunnel cases to `test/test_shows` (centre lit on frame 1, hue changes, both ends reached on 60 px within 200 iterations, symmetry without warp, asymmetry with `warp=2.0`) and factory cases to `test/test_show_factory` (defaults, clamping). Verify `pio test -e native -f test_shows -f test_show_factory` passes.

## 3. Echo show

- [x] 3.1 Add `src/show/Echo.{h,cpp}` (clamps `particles`/`speed`/`decay`/`hue_speed`, particles initialised on first execute from `Support::randomSeed()`, reflection at the ends, evenly spread hues, anti-aliased emission). Verify it builds natively.
- [x] 3.2 Add an `Echo` entry to `scripts/show_variants.json` (default `{particles:4, speed:1.0, decay:0.96, hue_speed:0.5}`, variants `swarm` and `ghost`) and register it in `ShowFactory.cpp`. Verify the generated header contains Echo and the canonical-default test covers it.
- [x] 3.3 Add Echo cases to `test/test_shows` (positions stay in range over 1000 iterations, identical output with the same seed, `wheel(0)`/`wheel(128)` for two particles, trail of at least 5 lit pixels with decreasing brightness) and factory cases (defaults, particle clamp). Verify `pio test -e native -f test_shows -f test_show_factory` passes.

## 4. Web UI

- [x] 4.1 In `data/control.html` add `tunnelParams` and `echoParams` sections (with `data-variants-for`), cases in `updateParameterVisibility()`, entries in `showsWithParams`, `applyTunnelParams()` / `applyEchoParams()` POSTing to `/api/show`, and cases in the load-current-params switch. Verify on a flashed device or with `scripts/compress_web.py` + the firmware build: selecting each show shows its section without starting it, the preset selector lists the manifest variants, and Apply starts the show.

## 5. Docs and gallery

- [x] 5.1 Add Tunnel and Echo to the show table and the show count in `README.md`, and their parameters to `docs/SHOW_PARAMETERS.md`. Verify that rendering the docs with `scripts/build_pages.py --landing` shows the new rows.
- [x] 5.2 Render the gallery with `scripts/build_pages.py --all docs/show_previews --seed 1` and verify that Tunnel and Echo PNGs (default and every variant) are produced and look right: rings stream outward for Tunnel, and trails follow the particles for Echo.

## 6. Integration checks

- [x] 6.1 Run `pio test -e native` (whole suite) and `pio run -e adafruit_qtpy_esp32s3_nopsram`. Verify that everything passes and record the RAM/flash usage delta from the build summary.
- [ ] 6.2 On the device with 300 LEDs, run Tunnel with `warp=2.5`, then switch to another show and back several times. Verify the frame rate shows no visible stutter and free heap returns to its baseline after switching away (check the serial log at `CORE_DEBUG_LEVEL=4`).
- [x] 6.3 Run `openspec validate add-feedback-shows --strict` and verify it passes.

## 7. Rework after device review

- [x] 7.1 Add `push` to `Support::FeedbackTransform` (outward offset on both sides of the centre, after zoom, before warp) and `test/test_feedback` cases for the push scenarios. Verify `pio test -e native -f test_feedback` passes.
- [x] 7.2 Add `push` and `pulse` to Tunnel (constructor, clamping, pulse intensity per the spec), the manifest, factory, simulator parameter table, UI section and load switch, and docs. Add Tunnel test cases for pulses and continuous emission, and factory cases for the new defaults and clamps. Verify `pio test -e native -f test_shows -f test_show_factory` passes.
- [x] 7.3 Give Echo per-particle speeds and swept sub-step emission (at most 0.25 px, reflecting per sub-step). Add Echo test cases for the speeds and the stripe-free trail. Remove the "0.5 or whole numbers" speed advice from the UI hint and docs. Verify `pio test -e native -f test_shows` passes.
- [x] 7.4 Rebuild the simulator explicitly (`pio run -e native_show_sim`; `build_pages.py` reuses a stale binary), re-render the gallery with `--all docs/show_previews --seed 1 --landing`, and verify that Tunnel shows distinct rings flying outward in every variant and that Echo trails are stripe-free and cross at varying angles. Retune defaults/variants if not, keeping spec, manifest, factory, UI and docs in sync.
- [x] 7.5 Re-run the whole native suite, the firmware build, clang-format, and `openspec validate add-feedback-shows --strict`. Verify everything passes and record the flash delta.
- [x] 7.6 After review, clear the centre pixel pair on even-length strips (`|x - c| <= 0.5`) so it cannot accumulate, and split Echo's sub-step emission at a bounce so the turn-around pixel is not skipped. Add feedback and Tunnel regression tests. Verify `pio test -e native -f test_feedback -f test_shows` passes.

## Workflow follow-up

- Archive the change with `/opsx:archive` after review and merge.
