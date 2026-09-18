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
     * oscillating source (in units of wavelength, so the falloff is independent
     * of strip length), and the wave's brightness contribution is the absolute
     * value of a signed sine so the whole strip remains lit at all times. An
     * end-fade factor dims the source as it approaches each strip end, hiding
     * the hot spot that would otherwise appear because the cosine source
     * momentarily stops at the extremes.
     */
    class Wave : public Show {
    private:
        float decay_rate; // Exponential decay per wavelength of distance from the source (higher = faster falloff)
        float brightness_frequency; // Source bounce frequency in cycles per second
        float wavelength; // Wave wavelength in pixels

        float time; // Time counter for source motion
        float color_time; // Time counter for color cycling along the strip

    public:
        /**
         * Constructor with configurable parameters.
         * @param decay_rate Exponential decay per wavelength of distance from the oscillating source (default: 1.0).
         * @param brightness_frequency Source bounce frequency in cycles per second (default: 0.07).
         * @param wavelength Wave wavelength in pixels (default: 15.0).
         */
        Wave(float decay_rate = 1.0f,
             float brightness_frequency = 0.07f,
             float wavelength = 15.0f);

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