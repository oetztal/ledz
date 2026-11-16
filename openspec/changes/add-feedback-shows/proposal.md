# Proposal

## Why

Every current show computes each frame from scratch, so none of them has the
trails, tunnels and smearing that come from feeding the previous frame back
into the next. That technique is what makes MilkDrop/butterchurn presets look
alive. A small 1D version of it would let us build a family of new shows from
short "emitter" functions instead of writing each effect from scratch.

## What Changes

- Add a reusable 1D feedback buffer. Each frame it remaps the previous frame
  through a configurable transform (zoom about the centre, scroll, sine warp,
  mirror fold), multiplies it by a decay factor, and lets the show add new
  light on top before writing the result to the strip.
- Add a **Tunnel** show: a hue-cycling emitter at the strip centre with
  zoom < 1, so rings of colour stream outward toward both ends, with an
  optional sine warp for a liquid wobble.
- Add an **Echo** show: several particles bounce between the strip ends and
  leave long, decaying comet trails behind them.
- Register both shows in the show factory, the variants manifest
  (`scripts/show_variants.json`, with a default and curated presets), the
  web UI parameter sections, and the show simulator / Pages gallery.

## Capabilities

### New Capabilities
- `feedback-rendering`: frame-to-frame feedback semantics shared by
  feedback-based shows. Covers the transforms, decay, additive emission,
  edge handling, conversion to 8-bit output, and memory bounds.
- `tunnel-show`: the Tunnel show's visual behaviour and its JSON parameters.
- `echo-show`: the Echo show's visual behaviour and its JSON parameters.

### Modified Capabilities
<!-- None: adding manifest entries and UI sections for new shows follows the
     existing canonical-show-config and web-ui-controls requirements without
     changing them. -->

## Impact

- New code: `src/support/Feedback.{h,cpp}`, `src/show/Tunnel.{h,cpp}`,
  `src/show/Echo.{h,cpp}`.
- Modified: `src/show/factory/ShowFactory.cpp` (two registrations),
  `scripts/show_variants.json` (two entries, regenerated header/JS payload),
  `data/control.html` (two parameter sections, `updateParameterVisibility()`,
  `showsWithParams`, apply/load functions).
- Tests: new native suite `test/test_feedback`, plus Tunnel/Echo cases in
  `test/test_shows` and `test/test_show_factory`.
- Memory: two float RGB buffers per active feedback show (24 bytes/LED,
  7.2 KB at the default 300 LEDs and 24 KB at the 1000-LED maximum). They are
  freed when the show is replaced. No new dependencies.
- Flash: a few KB of code plus the UI markup.
