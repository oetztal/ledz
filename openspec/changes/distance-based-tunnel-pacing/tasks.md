# Tasks

## 1. Tunnel pacing model

- [x] 1.1 Add `Tunnel::frameDecay()` returning `decay ^ push` for `push > 0` and
  `decay` for `push == 0`, per design.md. Update the constructor doc comment and
  use it in `execute()`.
- [x] 1.2 Replace the frame-driven pulse with an accumulated travel distance:
  `pulseIntensity(float travel)`, `emission_distance_ += push`, and keep
  `frame_` for sway and the `push == 0` fallback. Add an `emissionDistance()`
  getter.

## 2. Tests

- [x] 2.1 Add `test/test_shows` cases for the `frameDecay()` value at
  `push == 0.5`, the `push == 0` fallback, a slow tunnel reaching both ends,
  distance-based `pulseIntensity`, accumulated travel, and equal ring spacing
  at `push == 1` vs `push == 0.25` after equal travel. Verify
  `pio test -e native -f test_shows` passes.

## 3. Docs, UI and presets

- [x] 3.1 Update the Tunnel `decay` and `pulse` descriptions in
  `docs/SHOW_PARAMETERS.md` and the hints in `data/control.html` to per-pixel
  wording. No default or range changes.
- [x] 3.2 Retune `hyperspace` in `scripts/show_variants.json` from `pulse: 12`
  to `pulse: 24` so its 24-pixel ring spacing is preserved.

## 4. Verification

- [x] 4.1 Run `pio test -e native` and `pio run -e adafruit_qtpy_esp32s3_nopsram`.
- [x] 4.2 Run `openspec validate distance-based-tunnel-pacing --strict`.
- [x] 4.3 Re-render the Tunnel gallery previews and check the default and both
  variants still look right.
