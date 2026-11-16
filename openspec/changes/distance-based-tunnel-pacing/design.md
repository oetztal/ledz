# Design

## Current behaviour

`Tunnel::execute()` passes `decay_` straight to `Support::Feedback::begin()`,
which multiplies every sampled pixel by that factor once per iteration. The
emission intensity comes from `pulseIntensity(frame_)`, a raised cosine with a
period of `pulse` frames. For a ring emitted at the centre:

- intensity after `k` frames is `I0 * decay^k`;
- it has travelled about `k * push` pixels, so its fade distance is
  proportional to `push`;
- successive flashes are `pulse` frames apart, leaving `push * pulse` pixels
  between rings.

Slowing the tunnel with a smaller `push` therefore shortens rings and packs them
closer. On a 60-pixel strip, `push = 0.25` makes a ring die after ~51 pixels and
puts rings ~5 pixels apart instead of ~20.

## Change

Keep `decay` as the per-pixel attenuation `a` and `pulse` as the per-pixel
spacing `P`. Maintain a float accumulator `emission_distance_` that grows by
`push` each iteration, and use it instead of the frame counter to drive the
pulse:

```cpp
const float travel = push_ > 0.0f ? emission_distance_ : static_cast<float>(frame_);
const float intensity = pulseIntensity(travel);
...
emission_distance_ += push_;
```

The per-frame decay passed to feedback is `decay ^ push`:

```cpp
float Tunnel::frameDecay() const {
    return push_ > 0.0f ? std::pow(decay_, push_) : decay_;
}
```

### Why fractional push softens the rings

The feedback buffers are one float per pixel, so moving content by a fractional
`push` resamples it every frame and slightly blurs it. A slow tunnel therefore
has softer, broader rings than a fast one; the ring positions still follow the
per-pixel pacing. Sharpening or supersampling is out of scope for this change.

### Why `push == 0` keeps the per-frame values

With `push == 0` the travel distance never grows, so a strict distance-based
pulse would freeze at its first phase (full intensity) and a strict distance
decay would be `1`: the centre would accumulate light until it saturates. The
existing frame-driven pulse and decay are more useful there (a slowly fading
glow that pulses over time), so they are kept as an explicit fallback.

### Why zoom is not included in the distance

`zoom < 1` also moves content outward (by `(r)(1 - zoom)` at radius `r`), but
including it in the attenuation or pulse phase would make rings fade and repeat
faster near the ends and change the default look, where the same parameters were
tuned with zoom in play. Zoom remains a geometric stretch on top of transport;
only `push` defines the pacing. This keeps `decay = 0.97, push = 1` pixel
identical to today.

## Alternatives considered

- **New `decay_distance` / `pulse_distance` parameters, keep the old ones.** Avoids
  a semantic change but doubles the parameter set and leaves the coupling in
  place for anyone still using the old knobs. Rejected: the point is that users
  should not have to tune pairs together.
- **Compute an integer frame period `round(pulse / push)`.** Cannot express
  fractional spacing and makes ring spacing jump as `push` changes. Rejected in
  favour of the float travel accumulator.
- **Emit one discrete flash when travel crosses a multiple of `pulse`.**
  Gives constant spacing but loses the smooth pulse envelope and makes ring
  thickness speed-dependent. Rejected: the envelope in distance also gives a
  constant spatial thickness.
- **Per-destination decay using the local remap distance `|x - s|`, including
  zoom and warp.** More physically exact, but changes the default appearance
  and needs `Feedback` to know about transport semantics. Rejected for this
  change; could be a later refinement.

## Non-goals

- `hue_speed` remains per frame, so a slower tunnel changes colour faster per
  ring than the default. Making hue distance-based is a natural follow-up but is
  not part of this change.
- Sway and warp phases remain time-based, so the tunnel bends at the same
  real-time rate regardless of travel speed. That matches their "slow
  atmospheric drift" intent.
- Echo's trail decay remains per-frame; its particles already emit swept paths,
  and its decay semantics are documented separately. A follow-up could apply
  the same idea there.

## Risks

- Presets with `push != 1` (`hyperspace`, `push = 2`, old `pulse = 12`) now
  repeat every 12 pixels instead of every 24. Its manifest entry is retuned to
  `pulse = 24` to keep the previous spacing; re-render the gallery and retune if
  a variant looks wrong.
- Saved NVS params are unaffected: names, defaults and ranges do not change.
- `std::pow` and one float accumulation per frame are negligible next to the
  `3 * N` float work already done each frame.
