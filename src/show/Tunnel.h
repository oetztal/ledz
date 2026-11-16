#ifndef LEDZ_TUNNEL_H
#define LEDZ_TUNNEL_H

#include "Show.h"
#include "support/Feedback.h"

namespace Show {
    /**
     * Tunnel - rings of colour flying outward from the strip centre.
     *
     * Each iteration a hue-cycling light is emitted at the centre and the
     * previous frame is pushed and zoomed outward through Support::Feedback,
     * so earlier emissions travel toward both ends while they decay. The
     * emission pulses, so each flash becomes a distinct ring. An optional sine
     * warp bends the rings for a liquid wobble.
     */
    class Tunnel : public Show {
        float zoom_;
        float decay_;
        float hue_speed_;
        float warp_;
        float push_;
        int pulse_;

        float hue_ = 0.0f;
        uint32_t frame_ = 0; // drives the pulse; counts this show's iterations
        Support::Feedback feedback_;

    public:
        /**
         * @param zoom feedback zoom about the centre, clamped to [0.5, 1.0]
         * @param decay per-iteration decay, clamped to [0.0, 1.0]
         * @param hue_speed wheel steps per iteration, clamped to [0.0, 16.0]
         * @param warp warp amplitude in pixels, clamped to [0.0, 8.0]
         * @param push outward push in pixels per iteration, clamped to [0.0, 4.0]
         * @param pulse iterations between flashes, clamped to [0, 200]; 0 emits continuously
         */
        Tunnel(float zoom, float decay, float hue_speed, float warp, float push, int pulse);

        void execute(Strip::Strip& strip, Iteration iteration) override;

        [[nodiscard]] float zoom() const { return zoom_; }
        [[nodiscard]] float decay() const { return decay_; }
        [[nodiscard]] float hueSpeed() const { return hue_speed_; }
        [[nodiscard]] float warp() const { return warp_; }
        [[nodiscard]] float push() const { return push_; }
        [[nodiscard]] int pulse() const { return pulse_; }

        /** Emission intensity for the show's frame @p frame, per the pulse shape. */
        [[nodiscard]] float pulseIntensity(uint32_t frame) const;
    };
} // namespace Show

#endif // LEDZ_TUNNEL_H
