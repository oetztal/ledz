#ifndef LEDZ_WAVE_H
#define LEDZ_WAVE_H

#include "Show.h"

namespace Show {
    /**
     * Wave - A wave whose source oscillates along the strip.
     *
     * The source position follows a cosine bounce between the two ends of the
     * strip, so wavefronts are emitted in alternating directions. Where
     * previously emitted wavefronts still propagate, the wavefronts overlap and
     * self-interfere. Brightness decays exponentially with distance from the
     * oscillating source (symmetric in both directions), and the source's own
     * brightness oscillates subtly so the whole strip remains lit at all times.
     *
     * Two modes are accepted by the JSON contract (Bounce, Traveling) but
     * currently produce identical output - the wavelength-based stripe layer
     * they used to differentiate was removed to match the reference
     * implementation in scripts/build_pages.py, and no replacement modulation
     * has been added. The mode parameter is kept for future expansion.
     */
    enum class WaveMode { Bounce, Traveling };

    class Wave : public Show {
    private:
        float decay_rate; // Rate of brightness decay towards ends (higher = faster decay)
        float brightness_frequency; // Frequency of brightness oscillation at source
        WaveMode mode; // Phase behaviour (currently unused; kept for future expansion)

        float time = 0.0f; // Time counter for wave position
        float color_time = 0.0f; // Time counter for color cycling

    public:
        /**
         * Constructor with configurable parameters.
         * @param decay_rate Rate of brightness decay (default: 2.0).
         * @param brightness_frequency Frequency of source brightness oscillation (default: 0.1).
         * @param mode Phase mode: Bounce or Traveling. Currently a no-op; both
         *             modes render identically. Kept for future expansion.
         */
        Wave(float decay_rate,
             float brightness_frequency,
             WaveMode mode);

        /**
         * Execute the show - update wave animation
         * @param strip LED strip to control
         * @param iteration Current iteration number
         */
        void execute(Strip::Strip &strip, Iteration iteration) override;

        const char *name() { return "Wave"; }
    };
} // namespace Show

#endif //LEDZ_WAVE_H
