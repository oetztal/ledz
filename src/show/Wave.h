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
     * oscillating source (symmetric in both directions), and the wave's
     * brightness contribution is the absolute value of a signed sine so the
     * whole strip remains lit at all times.
     *
     * Two modes are available. Bounce mode keeps stripes phase-locked to the
     * source (they move with the source). Traveling mode detaches the stripes
     * from the source so they drift across the strip at
     * `brightness_frequency * wavelength` pixels per second, while the
     * bouncing brightness envelope still applies.
     */
    enum class WaveMode { Bounce, Traveling };

    class Wave : public Show {
    private:
        float decay_rate; // Rate of brightness decay towards ends (higher = faster decay)
        float brightness_frequency; // Frequency of brightness oscillation at source
        float wavelength; // Wavelength of the wave pattern (higher = longer waves)
        WaveMode mode; // Phase behaviour (Bounce = source-relative, Traveling = drifting)

        float time; // Time counter for wave position
        float color_time; // Time counter for color cycling

    public:
        /**
         * Constructor with configurable parameters.
         * @param decay_rate Rate of brightness decay (default: 2.0).
         * @param brightness_frequency Frequency of source brightness oscillation (default: 0.1).
         * @param wavelength Wavelength of wave pattern (default: 6.0).
         * @param mode Phase mode: Bounce (stripes follow the source) or
         *             Traveling (stripes drift independently of the source).
         */
        Wave(float decay_rate = 2.0f,
             float brightness_frequency = 0.1f,
             float wavelength = 6.0f,
             WaveMode mode = WaveMode::Bounce);

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