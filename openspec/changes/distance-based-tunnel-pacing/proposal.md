# Proposal

## Why

Tunnel's `decay` is applied once per rendered frame, so the distance a ring
travels before it fades is `push * frames`, where `frames` is fixed by `decay`.
Slowing the tunnel with a smaller `push` therefore shortens every ring's life in
pixels as well as in time: a ring that reaches the strip ends at `push = 1` dies
halfway there at `push = 0.5`. Users who want a slower tunnel must raise `decay`
to compensate, and there is no obvious value to pick.

Ring spacing has the same coupling: a flash every `pulse` frames leaves
`push * pulse` pixels between rings, so slowing `push` packs the rings closer
together.

Expressing both attenuation and pulse pacing per pixel of travel removes the
coupling: the same `decay` and `pulse` give the same visible ring length,
thickness and spacing at any `push`.

## What Changes

- Reinterpret Tunnel's `decay` parameter as **brightness kept per pixel of
  outward travel**, not per frame.
- Reinterpret Tunnel's `pulse` parameter as **pixels of travel between ring
  flashes**, not frames.
- Drive the pulse from an accumulated travel distance (`push` per iteration),
  so ring spacing and spatial thickness follow distance instead of frames.
- Keep `decay` and `pulse` as per-frame values when `push == 0`, because
  zero-distance pacing would freeze the pulse and never fade, so the centre
  would accumulate light forever.
- Document the new meanings in `docs/SHOW_PARAMETERS.md` and the web UI hints.
- Add Tunnel tests for speed-invariant ring extent, spacing and the `push == 0`
  fallback.

## Capabilities

### Modified Capabilities

- `tunnel-show`: `decay` and `pulse` are applied per pixel of travel rather than
  per iteration, so ring length, thickness and spacing are independent of
  `push`.

## Impact

- `src/show/Tunnel.{h,cpp}`: a travel accumulator and the converted decay
  factor passed to `Support::Feedback::begin`.
- `docs/SHOW_PARAMETERS.md`, `data/control.html`: wording only.
- `scripts/show_variants.json`: `hyperspace`'s `pulse` is retuned to preserve
  its previous 24-pixel ring spacing.
- `test/test_shows`: new cases.
- Behaviour at `push = 1` is unchanged. At other speeds, rings now fade and
  repeat after the same distance instead of the same number of frames;
  curated presets may need a visual pass.
