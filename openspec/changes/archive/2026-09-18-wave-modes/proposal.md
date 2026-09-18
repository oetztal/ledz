## Why

The Wave show currently has one behavior: a bouncing source whose stripes phase-lock to the source position. The `wave-revert` change (archived earlier today) restored this implementation after `wave-polish` over-corrected it, and the spec captures the current "bounce" behavior precisely. The user wants to expand what Wave can express without disturbing the now-stable default.

Two side observations inform the scope:

1. The web UI has a `waveSpeed` input that maps to a `wave_speed` field the factory has been silently ignoring since `wave-interference`. Users adjusting it see no effect. We should clean that up at the same time.
2. The `wave-revert` spec locks in "no new web UI parameters". That gate needs to come off for this change.

This change adds a `mode` parameter with two values (`bounce` = today's behavior; `traveling` = new). Default stays `bounce`, so all existing NVS-stored configs and API clients behave identically.

## What Changes

- **Add `mode` parameter to Wave.** JSON: `{"mode": "bounce" | "traveling", ...}`. Default `"bounce"`. Unknown values fall back to `"bounce"` (same fallback semantics ArduinoJson already gives missing fields).
- **Implement `traveling` mode.** Stripes drift across the strip at `brightness_frequency × wavelength` pixels per second, while the bouncing brightness envelope still applies. The per-pixel cost is unchanged (one extra multiply and one extra multiply per pixel — both scalars computed once outside the loop).
- **Clean up the dead `waveSpeed` UI input.** Replace it with a `waveMode` dropdown. Update `applyWaveParams()` to send `mode` instead of `wave_speed`. Update `populateShowParams()` to read `mode` (and stop trying to read `wave_speed`).
- **Update `SHOW_PARAMETERS.md`.** Document the new `mode` parameter and both behaviors.
- **Update the `wave-show` spec.** Make bounce-specific requirements mode-conditional (`WHEN mode == "bounce" ...`). Add parallel requirements for traveling mode. Remove the "no new web UI parameters" requirement.

No **BREAKING** changes:
- NVS configs without `mode` load as `mode="bounce"`.
- API callers without `mode` get the same behavior as today.
- The `Wave` constructor's existing three-argument signature remains valid; `mode` is added as a defaulted fourth parameter.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `wave-show`: gains a `mode` parameter; bounce-specific requirements become mode-conditional; new requirements added for traveling mode; "no new web UI parameters" requirement is removed.

## Impact

- **Code**: `src/show/Wave.h` (add `WaveMode` enum, add `mode` field, add fourth ctor arg), `src/show/Wave.cpp` (add traveling branch in `execute()`), `src/ShowFactory.cpp` (parse `mode`, update description), `data/control.html` (replace `waveSpeed` input with `waveMode` dropdown; update `applyWaveParams()` and `populateShowParams()` Wave cases).
- **Build**: `data/control.html` → `src/generated/control_gz.h` is regenerated automatically by the existing build pipeline.
- **Docs**: `docs/SHOW_PARAMETERS.md` Wave section.
- **Tests**: existing four `Wave` tests in `test/test_shows/test_shows.cpp` and three in `test/test_show_factory/test_show_factory.cpp` are unchanged (default mode = bounce). Add 1–2 tests for traveling math and JSON parsing.
- **Spec**: `openspec/specs/wave-show/spec.md` is modified (this change produces the delta; the existing `wave-show/spec.md` is the base).