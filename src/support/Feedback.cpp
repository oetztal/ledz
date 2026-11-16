#include "Feedback.h"

#include <algorithm>
#include <cmath>
#include <new>
#include <utility>

#include "Color.h"
#include "Log.h"

[[maybe_unused]] static const char* const TAG = "show";

namespace Support {
    namespace {
        // 0.9f is slightly below 0.9, so 255 * 0.9f lands at 229.49999 and would
        // round down although the exact product rounds to 230. A slack far below
        // one output step absorbs that float error without keeping faded light
        // alive: anything under 0.5 - slack still rounds to 0.
        constexpr float kRoundingSlack = 1e-3f;

        Strip::ColorComponent toByte(float v) {
            v = std::clamp(v, 0.0f, 1.0f);
            return static_cast<Strip::ColorComponent>(v * 255.0f + 0.5f + kRoundingSlack);
        }
    } // namespace

    bool Feedback::resize(Strip::PixelIndex n) {
        const auto size = static_cast<std::size_t>(std::max<Strip::PixelIndex>(n, 0)) * 3;
        try {
            // Fresh vectors let the old buffers go even after a length
            // reduction, and leave the current state untouched if allocation
            // fails (the swap only happens once both vectors exist).
            std::vector<float> current(size, 0.0f);
            std::vector<float> previous(size, 0.0f);
            current_.swap(current);
            previous_.swap(previous);
        } catch (const std::bad_alloc&) {
            ESP_LOGE(TAG, "Feedback buffer allocation for %d pixels failed; keeping %d", n, length_);
            return false;
        }
        length_ = n;
        return true;
    }

    float Feedback::sample(float pos, int channel, bool wrap) const {
        const float n = static_cast<float>(length_);
        if (wrap) {
            pos = std::fmod(pos, n);
            if (pos < 0.0f) pos += n;
        } else if (pos < 0.0f || pos > n - 1.0f) {
            return 0.0f;
        }

        auto i0 = static_cast<Strip::PixelIndex>(pos);
        if (i0 >= length_) i0 = static_cast<Strip::PixelIndex>(length_ - 1); // fmod rounding at the top edge
        const float frac = pos - static_cast<float>(i0);
        const float v0 = previous_[static_cast<std::size_t>(i0) * 3 + channel];
        if (frac == 0.0f) {
            return v0;
        }
        Strip::PixelIndex i1 = i0 + 1;
        if (i1 >= length_) i1 = wrap ? 0 : i0;
        const float v1 = previous_[static_cast<std::size_t>(i1) * 3 + channel];
        return v0 + (v1 - v0) * frac;
    }

    void Feedback::begin(Strip::PixelIndex n, const FeedbackTransform& transform, float decay, uint64_t t) {
        // Compare against the requested length, not the allocated one, so a
        // failed allocation is retried only when the strip length changes again.
        if (n != requested_length_) {
            requested_length_ = n;
            resize(n);
        }
        std::swap(current_, previous_);
        if (length_ <= 0) {
            return;
        }

        decay = std::clamp(decay, 0.0f, 1.0f);
        const float c = (static_cast<float>(length_) - 1.0f) * 0.5f;
        const bool wrap = transform.scroll != 0.0f;
        const bool warp = transform.warp_amp != 0.0f;
        const auto phase = static_cast<float>(static_cast<double>(t) * transform.warp_speed);

        for (Strip::PixelIndex x = 0; x < length_; x++) {
            float xd = static_cast<float>(x);
            if (transform.mirror && xd > c) {
                xd = 2.0f * c - xd;
            }
            float s = c + (xd - c) * transform.zoom - transform.scroll;
            // Push samples closer to the centre, so content moves outward at a
            // constant speed even near the centre, where zoom barely moves it.
            if (xd > c) {
                s -= transform.push;
            } else if (xd < c) {
                s += transform.push;
            }
            if (warp) {
                s += transform.warp_amp * std::sin(transform.warp_freq * xd + phase);
            }
            const auto base = static_cast<std::size_t>(x) * 3;
            // Under push everything flows away from the centre and nothing
            // flows in, so the centre pixel(s) must not keep (and accumulate)
            // their light. Odd-length strips have one centre pixel; even-length
            // strips have a pair at c +/- 0.5 that would otherwise sample each
            // other and build up.
            const bool source = transform.push > 0.0f && std::fabs(xd - c) <= 0.5f;
            for (int ch = 0; ch < 3; ch++) {
                current_[base + ch] = source ? 0.0f : sample(s, ch, wrap) * decay;
            }
        }
    }

    void Feedback::addPixel(Strip::PixelIndex i, Strip::Color color, float intensity) {
        if (i < 0 || i >= length_ || intensity <= 0.0f) {
            return;
        }
        const auto base = static_cast<std::size_t>(i) * 3;
        const float scale = intensity / 255.0f;
        const Strip::ColorComponent rgb[3] = {Color::red(color), Color::green(color), Color::blue(color)};
        for (int ch = 0; ch < 3; ch++) {
            current_[base + ch] = std::min(1.0f, current_[base + ch] + static_cast<float>(rgb[ch]) * scale);
        }
    }

    void Feedback::add(float pos, Strip::Color color, float intensity) {
        if (!std::isfinite(pos)) {
            return;
        }
        const float floor_pos = std::floor(pos);
        const float frac = pos - floor_pos;
        const auto i0 = static_cast<Strip::PixelIndex>(floor_pos);
        addPixel(i0, color, intensity * (1.0f - frac));
        addPixel(static_cast<Strip::PixelIndex>(i0 + 1), color, intensity * frac);
    }

    void Feedback::render(Strip::Strip& strip) const {
        const Strip::PixelIndex n = strip.length();
        for (Strip::PixelIndex i = 0; i < n; i++) {
            if (i >= length_) {
                strip.setPixelColor(i, 0);
                continue;
            }
            const auto base = static_cast<std::size_t>(i) * 3;
            strip.setPixelColor(
                i, Color::from_rgb(toByte(current_[base]), toByte(current_[base + 1]), toByte(current_[base + 2])));
        }
    }

    float Feedback::intensity(Strip::PixelIndex i, int channel) const {
        if (i < 0 || i >= length_ || channel < 0 || channel > 2) {
            return 0.0f;
        }
        return current_[static_cast<std::size_t>(i) * 3 + channel];
    }
} // namespace Support
