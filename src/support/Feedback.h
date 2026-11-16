#ifndef LEDZ_SUPPORT_FEEDBACK_H
#define LEDZ_SUPPORT_FEEDBACK_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "strip/Strip.h"

namespace Support {
    /**
     * How the previous frame is remapped before it is decayed and new light is
     * added. For destination pixel x (after the optional mirror fold) the
     * previous frame is sampled at
     *
     *     c + (x - c) * zoom - sign(x - c) * push - scroll + warp_amp * sin(warp_freq * x + warp_speed * t)
     *
     * with c = (N - 1) / 2 + center. Under push the sample (before warp) never
     * crosses c - scroll; pixels nearer the centre than the push sample the centre.
     *
     * The default is the identity transform.
     */
    struct FeedbackTransform {
        float zoom = 1.0f;        // about the centre; < 1 moves content outward
        float scroll = 0.0f;      // px/iteration toward higher indices; wraps when non-zero
        float warp_amp = 0.0f;    // px; 0 disables the (sinf-heavy) warp term
        float warp_freq = 0.3f;   // rad/px
        float warp_speed = 0.05f; // rad/iteration
        bool mirror = false;      // fold the upper half onto the lower half
        float push = 0.0f;        // px/iteration outward from the centre, on both sides; clears the centre pixel(s)
        float center = 0.0f;      // px; shifts the centre c used by zoom, push and mirror
    };

    /**
     * 1D frame-to-frame feedback buffer shared by feedback-based shows.
     *
     * Each frame a show calls begin() to remap and decay the previous frame,
     * then add()/addPixel() to emit new light, then render() to write the
     * result to the strip. State is kept as float RGB in [0, 1] in two buffers
     * of 3 * N floats (24 bytes per LED) that are swapped every frame, so no
     * allocation happens unless the strip length changes.
     */
    class Feedback {
        std::vector<float> current_;              // frame being built (3 * N)
        std::vector<float> previous_;             // last frame, read by the remap (3 * N)
        Strip::PixelIndex length_ = 0;            // length the buffers hold
        Strip::PixelIndex requested_length_ = -1; // last length passed to begin()

        bool resize(Strip::PixelIndex n);
        [[nodiscard]] float sample(float pos, int channel, bool wrap) const;

    public:
        /**
         * Start a frame: remap the previous frame through @p transform and
         * multiply it by @p decay (clamped to [0, 1]).
         * @param n strip length; the buffers follow it, starting black
         * @param t iteration count, drives the warp phase
         */
        void begin(Strip::PixelIndex n, const FeedbackTransform& transform, float decay, uint64_t t);

        /**
         * Add light at a fractional position, split between the two nearest
         * pixels by their distance (anti-aliased). Channels saturate at 1.
         */
        void add(float pos, Strip::Color color, float intensity = 1.0f);

        /** Add light to a single pixel. Out-of-range indices are ignored. */
        void addPixel(Strip::PixelIndex i, Strip::Color color, float intensity = 1.0f);

        /** Write the current frame to @p strip, rounding to 8-bit channels. */
        void render(Strip::Strip& strip) const;

        /** Current intensity of one channel (0 = red, 1 = green, 2 = blue) of pixel @p i. */
        [[nodiscard]] float intensity(Strip::PixelIndex i, int channel) const;

        /** Number of floats held by each of the two buffers (3 * N). */
        [[nodiscard]] std::size_t bufferSize() const { return current_.size(); }
    };
} // namespace Support

#endif // LEDZ_SUPPORT_FEEDBACK_H
