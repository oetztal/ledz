# Support Classes

## SmoothBlend

The `SmoothBlend` class provides smooth color transitions for LED strips. It interpolates between initial colors and
target colors over a specified duration.

### Usage

#### Blend all LEDs to a single color

```cpp
#include "support/SmoothBlend.h"

// Create a blend to blue over 2 seconds (default duration)
Support::SmoothBlend blend(strip, 0x0000FF);

// In your loop, call step() repeatedly
while (blend.step()) {
    // The blend animates automatically
    // step() returns false when complete
}
```

#### Blend each LED to different colors

```cpp
#include "support/SmoothBlend.h"

// Set different target colors for each LED
std::vector<Strip::Color> targets;
for (int i = 0; i < strip.length(); i++) {
    targets.push_back(wheel(i * 255 / strip.length()));
}

// Create blend with custom duration (1 second)
Support::SmoothBlend blend(strip, targets, 1000);

// Animate
while (blend.step()) {
    delay(10); // Optional delay between steps
}
```

#### Check if blend is complete

```cpp
Support::SmoothBlend blend(strip, 0xFF0000, 500); // 500ms duration

if (blend.isComplete()) {
    // Blend finished
}
```

### Features

- **Linear interpolation**: Smooth color transitions using linear blending
- **Per-LED colors**: Each LED can have its own target color
- **Configurable duration**: Set blend duration in milliseconds (default: 2000ms)
- **Non-blocking**: Call `step()` in your loop for smooth animation
- **Platform independent**: Works on both Arduino and native platforms

### Implementation Notes

The class captures the initial colors from the strip when constructed, then smoothly blends to the target colors over
the specified duration. The blend progress is calculated using the `millis()` function for accurate timing.

The blend uses a linear interpolation formula:

```
blended_value = start_value * fade_progress + end_value * (1 - fade_progress)
```

Where `fade_progress` goes from 1.0 (at start) to 0.0 (at end).

## Feedback

`Support::Feedback` keeps a per-pixel float RGB frame between iterations, remaps
and decays it, lets a show add new light on top, and renders it to the strip. It
is the shared engine behind feedback-based shows such as Tunnel and Echo.

```cpp
#include "support/Feedback.h"

Support::Feedback feedback;

// Each frame:
Support::FeedbackTransform transform;
transform.zoom = 0.97f;   // < 1 pushes content outward from the centre
transform.push = 1.0f;    // constant outward speed near the centre
feedback.begin(strip.length(), transform, 0.93f, iteration);
feedback.addPixel((strip.length() - 1) / 2, color, 1.0f);
feedback.render(strip);
```

Available transforms are `zoom`, `scroll`, `warp_amp`/`warp_freq`/`warp_speed`,
`mirror`, `push`, and `center` (shifts the centre that zoom, push and mirror
work around). Emission can be added at a fractional position with
`add()` (anti-aliased) or at an integer pixel with `addPixel()`. Both buffers are
`3 * N` floats (24 bytes per LED) and are reallocated only when the strip length
changes.
