#ifndef LEDZ_ECHO_H
#define LEDZ_ECHO_H

#include <cstddef>
#include <vector>

#include "Show.h"
#include "support/Feedback.h"
#include "support/Random.h"

namespace Show {
    /**
     * Echo - particles bouncing between the strip ends, each dragging a long
     * comet trail.
     *
     * The particles move at different speeds on evenly spread hues and emit
     * light along their whole path each frame; the trails come from
     * Support::Feedback decaying the previous frame without any spatial
     * transform.
     */
    class Echo : public Show {
    public:
        struct Particle {
            float pos;
            float dir;   // +1 toward higher indices, -1 toward lower
            float speed; // pixels per iteration
        };

    private:
        int particle_count_;
        float speed_;
        float decay_;
        float hue_speed_;

        float drift_ = 0.0f;
        Strip::PixelIndex length_ = -1; // strip length the particles were placed on
        std::vector<Particle> particles_;
        Support::Random gen_;
        Support::Feedback feedback_;

        void placeParticles(Strip::PixelIndex n);
        static void advance(Particle& p, float distance, float last);

    public:
        /**
         * @param particles number of particles, clamped to [1, 8]
         * @param speed pixels per iteration, clamped to [0.05, 4.0]
         * @param decay per-iteration trail decay, clamped to [0.0, 1.0]
         * @param hue_speed wheel steps per iteration, clamped to [0.0, 16.0]
         */
        Echo(int particles, float speed, float decay, float hue_speed);

        void execute(Strip::Strip& strip, Iteration iteration) override;

        [[nodiscard]] int particleCount() const { return particle_count_; }
        [[nodiscard]] float speed() const { return speed_; }
        [[nodiscard]] float decay() const { return decay_; }
        [[nodiscard]] float hueSpeed() const { return hue_speed_; }

        /** Particle positions and directions; empty before the first execute(). */
        [[nodiscard]] const std::vector<Particle>& particles() const { return particles_; }

        /** Colour particle @p j currently emits. */
        [[nodiscard]] Strip::Color particleColor(std::size_t j) const;
    };
} // namespace Show

#endif // LEDZ_ECHO_H
