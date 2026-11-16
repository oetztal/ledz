#include "Tunnel.h"

#include <algorithm>
#include <cmath>

#include "support/Color.h"

namespace Show {
    Tunnel::Tunnel(float zoom, float decay, float hue_speed, float warp, float push, int pulse, float sway)
        : zoom_(std::clamp(zoom, 0.5f, 1.0f)), decay_(std::clamp(decay, 0.0f, 1.0f)),
          hue_speed_(std::clamp(hue_speed, 0.0f, 16.0f)), warp_(std::clamp(warp, 0.0f, 8.0f)),
          push_(std::clamp(push, 0.0f, 4.0f)), pulse_(std::clamp(pulse, 0, 200)), sway_(std::clamp(sway, 0.0f, 1.0f)) {}

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

    float Tunnel::swayOffset(uint32_t frame, Strip::PixelIndex n) const {
        if (sway_ == 0.0f || n < 2) {
            return 0.0f;
        }
        // Two incommensurate sines (periods of about 4.2 s and 1.7 s at 100 Hz)
        // so the drift never settles into an obvious loop. Their sum stays
        // within +/-1.5; scale it to +/-sway of the half length.
        const auto f = static_cast<float>(frame);
        const float wave = std::sin(f * 0.015f) + 0.5f * std::sin(f * 0.037f + 1.0f);
        const float half = (static_cast<float>(n) - 1.0f) * 0.5f;
        return sway_ * half * wave / 1.5f;
    }

    void Tunnel::execute(Strip::Strip& strip, Iteration iteration) {
        const Strip::PixelIndex n = strip.length();

        Support::FeedbackTransform transform;
        transform.zoom = zoom_;
        transform.warp_amp = warp_;
        transform.push = push_;
        transform.center = swayOffset(frame_, n);
        // Without warp or sway the remap is symmetric anyway; folding makes that
        // exact instead of leaving it to float rounding in the interpolation.
        transform.mirror = warp_ == 0.0f && sway_ == 0.0f;
        feedback_.begin(n, transform, decay_, iteration);

        if (n > 0) {
            const Strip::Color color = Support::Color::wheel(hue_);
            const float intensity = pulseIntensity(frame_);
            if (transform.center == 0.0f) {
                // One centre pixel for odd lengths, the two middle pixels for
                // even ones, so the emission itself is symmetric.
                feedback_.addPixel(static_cast<Strip::PixelIndex>((n - 1) / 2), color, intensity);
                if (n % 2 == 0) {
                    feedback_.addPixel(static_cast<Strip::PixelIndex>(n / 2), color, intensity);
                }
            } else {
                // A two-pixel-wide anti-aliased emission at the drifting centre.
                const float c = (static_cast<float>(n) - 1.0f) * 0.5f + transform.center;
                feedback_.add(c - 0.5f, color, intensity);
                feedback_.add(c + 0.5f, color, intensity);
            }
        }
        feedback_.render(strip);

        hue_ = std::fmod(hue_ + hue_speed_, 256.0f);
        frame_++;
    }
} // namespace Show
