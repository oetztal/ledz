## Why

`scripts/wave_show.py` is the canonical reference implementation of the Wave show, maintained alongside the firmware to preview parameter sets and validate new behaviour before any C++ change. The current C++ implementation has drifted from that reference in two ways: it still carries the `wavelength` parameter and its associated `|sin(phase)|` modulation (the reference explicitly omits both because the resulting stripes are too short to be useful on typical strip widths), and it uses the Adafruit edge-walking `wheel()` that cannot produce pure yellow / cyan / magenta. The reference uses an HSV cube-walking `wheel()` that hits all six primary and secondary colours. This change brings the C++ implementation back in line with the reference.

## What Changes

- **BREAKING**: Remove the `wavelength` parameter from the Wave show (constructor, factory JSON parsing, web UI, docs, tests). The `|sin(phase)|` wave-amplitude modulation that depended on it is also removed; brightness now comes from `source_brightness * envelope` alone.
- **BREAKING**: Replace the Adafruit edge-walking `wheel()` in `src/color.cpp` with the HSV cube-walking version from `scripts/wave_show.py`. The new wheel produces pure yellow at hue ~42, cyan at ~127, and magenta at ~212; the old wheel only ever had one of the three RGB channels at full intensity at any position.
- Keep the `mode` parameter (`bounce` / `traveling`) accepted by the JSON contract for future expansion; with the wavelength modulation gone, both modes render identically today, matching the reference.
- Update the Wave spec to drop the wavelength requirements and the wavelength-dependent scenarios.
- No NVS migration is needed: `wavelength` is dropped via ArduinoJson's `|` operator (missing → default, which is now meaningless and ignored), and the wheel change is purely visual.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `wave-show`: `wavelength` parameter removed from the accepted parameter list; the `|sin(phase)|` wave amplitude requirement is replaced with a plain `source_brightness * envelope` requirement; `mode` is documented as currently a no-op; the per-mode phase scenarios are removed.
- `web-ui-controls`: the Wave parameter section drops the Wavelength input and its label/help text; the Apply Parameters payload drops the `wavelength` field.

## Impact

- **Code**: `src/show/Wave.h` (wavelength member + ctor parameter removed), `src/show/Wave.cpp` (wavelength-based phase + `|sin(phase)|` removed), `src/show/factory/ShowFactory.cpp` (wavelength parsing + log line dropped), `src/color.cpp` (wheel replaced with HSV cube-walking version).
- **Shared visual change**: `src/color.cpp`'s `wheel()` is used by Rainbow, Mandelbrot, Chaos, MorseCode, and TheaterChase in addition to Wave. All five shows get the richer six-colour palette; none get a parameter or contract change.
- **Web UI**: `data/control.html` drops the Wavelength input from the Wave parameter section and from `applyWaveParams()` and `populateShowParams()`.
- **Docs**: `docs/SHOW_PARAMETERS.md` Wave section rewritten to drop wavelength and to note that `mode` is currently a no-op.
- **Tests**: `test/test_shows/test_shows.cpp` drops the wavelength argument from `Show::Wave` constructions and the traveling-mode drift test (no phase to drift without wavelength); `test/test_show_factory/test_show_factory.cpp` drops the wavelength-related scenarios and replaces the mode-difference scenario with one that asserts both modes produce identical output.
- **Spec**: `openspec/specs/wave-show/spec.md` drops wavelength requirements and scenarios; `openspec/specs/web-ui-controls/spec.md` drops the wavelength UI requirements.
- **Existing users**: anyone with `wavelength` in their stored Wave config sees it silently dropped (the field is ignored, default is meaningless); everyone gets the richer wheel palette on next show change or reboot.
