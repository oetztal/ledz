#include "Echo.h"

#include <algorithm>
#include <cmath>
#include <random>

#include "support/Color.h"

namespace Show {
    Echo::Echo(int particles, float speed, float decay, float hue_speed)
        : particle_count_(std::clamp(particles, 1, 8)), speed_(std::clamp(speed, 0.05f, 4.0f)),
          decay_(std::clamp(decay, 0.0f, 1.0f)), hue_speed_(std::clamp(hue_speed, 0.0f, 16.0f)) {}

    void Echo::placeParticles(Strip::PixelIndex n) {
        if (length_ < 0) {
            gen_.seed(Support::randomSeed());
        }
        const float last = static_cast<float>(std::max<Strip::PixelIndex>(n - 1, 0));
        std::uniform_real_distribution<float> position(0.0f, last);
        std::bernoulli_distribution forward(0.5);
        particles_.clear();
        for (int j = 0; j < particle_count_; j++) {
            const float pos = position(gen_);
            const float dir = forward(gen_) ? 1.0f : -1.0f;
            // Speeds spread evenly over 0.6..1.4 x speed, so the trails cross
            // at varying angles instead of forming a regular lattice.
            const float factor = particle_count_ == 1
                                     ? 1.0f
                                     : 0.6f + 0.8f * static_cast<float>(j) / static_cast<float>(particle_count_ - 1);
            particles_.push_back({pos, dir, speed_ * factor});
        }
        length_ = n;
    }

    Strip::Color Echo::particleColor(std::size_t j) const {
        const float base = static_cast<float>(j) * 256.0f / static_cast<float>(particle_count_);
        return Support::Color::wheel(std::fmod(base + drift_, 256.0f));
    }

    void Echo::advance(Particle& p, float distance, float last) {
        p.pos += p.dir * distance;
        // Reflect off the ends; a loop because a step on a very short strip can
        // overshoot by more than the strip length.
        while (p.pos < 0.0f || p.pos > last) {
            if (last == 0.0f) {
                p.pos = 0.0f;
            } else if (p.pos < 0.0f) {
                p.pos = -p.pos;
                p.dir = 1.0f;
            } else {
                p.pos = 2.0f * last - p.pos;
                p.dir = -1.0f;
            }
        }
    }

    void Echo::execute(Strip::Strip& strip, Iteration iteration) {
        const Strip::PixelIndex n = strip.length();
        // The strip length is only known here, so particles are placed on the
        // first frame and again if the strip is resized.
        if (n != length_) {
            placeParticles(n);
        }

        feedback_.begin(n, Support::FeedbackTransform{}, decay_, iteration);

        // Emit along the whole path travelled this frame, in sub-steps of at
        // most kMaxStep px with intensity equal to the step length. Every
        // pixel passed then receives about one unit of light at any speed;
        // emitting only at the end position leaves stripes at fractional
        // speeds, because pixels would be lit on differing numbers of frames.
        constexpr float kMaxStep = 0.25f;
        const float last = static_cast<float>(std::max<Strip::PixelIndex>(n - 1, 0));
        for (std::size_t j = 0; j < particles_.size(); j++) {
            Particle& p = particles_[j];
            const Strip::Color color = particleColor(j);
            const int steps = std::max(1, static_cast<int>(std::ceil(p.speed / kMaxStep)));
            const float step = p.speed / static_cast<float>(steps);
            for (int k = 0; k < steps; k++) {
                const float from = p.pos;
                const float dir = p.dir;
                advance(p, step, last);
                if (p.dir != dir) {
                    // A reflection happened inside this sub-step. Emit the part
                    // of the path up to the end and the part back from it, so
                    // the bounce pixel is not skipped.
                    const float wall = dir > 0.0f ? last : 0.0f;
                    const float to_wall = std::fabs(wall - from);
                    feedback_.add(wall, color, to_wall);
                    feedback_.add(p.pos, color, step - to_wall);
                } else {
                    feedback_.add(p.pos, color, step);
                }
            }
        }
        feedback_.render(strip);

        drift_ = std::fmod(drift_ + hue_speed_, 256.0f);
    }
} // namespace Show
