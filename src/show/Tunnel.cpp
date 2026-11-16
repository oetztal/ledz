#include "Tunnel.h"

#include <algorithm>
#include <cmath>

#include "support/Color.h"

namespace Show {
    Tunnel::Tunnel(float zoom, float decay, float hue_speed, float warp, float push, int pulse)
        : zoom_(std::clamp(zoom, 0.5f, 1.0f)), decay_(std::clamp(decay, 0.0f, 1.0f)),
          hue_speed_(std::clamp(hue_speed, 0.0f, 16.0f)), warp_(std::clamp(warp, 0.0f, 8.0f)),
          push_(std::clamp(push, 0.0f, 4.0f)), pulse_(std::clamp(pulse, 0, 200)) {}

    float Tunnel::pulseIntensity(uint32_t frame) const {
        if (pulse_ == 0) {
            return 1.0f;
        }
        // A raised cosine sharpened by the 6th power: a short flash once per
        // period and a dim glow in between, so each flash becomes a ring.
        constexpr float kTwoPi = 6.28318530718f;
        const float phase = static_cast<float>(frame % static_cast<uint32_t>(pulse_)) / static_cast<float>(pulse_);
        const float wave = 0.5f + 0.5f * std::cos(kTwoPi * phase);
        return 0.08f + 0.92f * std::pow(wave, 6.0f);
    }

    void Tunnel::execute(Strip::Strip& strip, Iteration iteration) {
        const Strip::PixelIndex n = strip.length();

        Support::FeedbackTransform transform;
        transform.zoom = zoom_;
        transform.warp_amp = warp_;
        transform.push = push_;
        // Without warp the remap is symmetric anyway; folding makes that exact
        // instead of leaving it to float rounding in the interpolation.
        transform.mirror = warp_ == 0.0f;
        feedback_.begin(n, transform, decay_, iteration);

        if (n > 0) {
            // One centre pixel for odd lengths, the two middle pixels for even
            // ones, so the emission itself is symmetric.
            const Strip::Color color = Support::Color::wheel(hue_);
            const float intensity = pulseIntensity(frame_);
            feedback_.addPixel(static_cast<Strip::PixelIndex>((n - 1) / 2), color, intensity);
            if (n % 2 == 0) {
                feedback_.addPixel(static_cast<Strip::PixelIndex>(n / 2), color, intensity);
            }
        }
        feedback_.render(strip);

        hue_ = std::fmod(hue_ + hue_speed_, 256.0f);
        frame_++;
    }
} // namespace Show
