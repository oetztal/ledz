## Why

The Rainbow show currently runs with one fixed animation behavior: hue advances by 1 per frame and 1 per pixel. Users have no way to dial in the speed of the scroll or the density of the color bands without rebuilding the firmware. Making these two knobs configurable lets users tune the effect to their strip length, mood, or ambient lighting without code changes.

## What Changes

- **Rainbow show gains two float parameters**: `time_step` (hue change per frame) and `pixel_step` (hue change per pixel). Both default to `1.0`, preserving the current animation exactly.
- **Web UI gains a Rainbow params section** with two range sliders (0.0–5.0, step 0.05) and an Apply button, following the same pattern as `TheaterChase` and `Wave`.
- **ShowFactory parses the new params** using the existing `doc["key"] | default` pattern. Devices with no stored params (or `{}`) continue to get the current behavior.
- **Documentation updated**: `docs/SHOW_PARAMETERS.md` entry for Rainbow is replaced from "no parameters" to a full description.
- **Touch controller untouched**: `RAINBOW_VARIANTS[]` stays at a single `{}` entry; the touch button will continue to cycle into Rainbow with default behavior.

## Capabilities

### New Capabilities
- `rainbow-show`: Describes the Rainbow LED animation's configurable behavior — two float parameters controlling hue advance per frame and per pixel, with default values matching the prior fixed behavior.

### Modified Capabilities
<!-- No existing specs yet. -->
- *(none)*

## Impact

- `src/show/Rainbow.h` — add fields and constructor with defaults
- `src/show/Rainbow.cpp` — replace fixed `(iteration + index) % 255` with `(iteration * time_step + index * pixel_step)` wrapped via `fmodf`
- `src/ShowFactory.cpp` — parse `time_step` and `pixel_step` from JSON
- `data/control.html` — add `rainbowParams` section, `applyRainbowParams()` function, `updateParameterVisibility()` case, and `populateShowParams()` case; add `Rainbow` to `showsWithParams` list
- `docs/SHOW_PARAMETERS.md` — replace the "Rainbow, ColorRun, Jump currently don't support parameters" sentence with a Rainbow parameters section
- `test/test_shows/test_shows.cpp` — add basic coverage for Rainbow parameter handling

Backward compatible: any device with stored `params_json` of `{}` (or no stored params) will reconstruct Rainbow with `time_step=1.0, pixel_step=1.0`, identical to current behavior. No NVS migration required.