#include "Wave.h"
#include "../support/Color.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Show {
    Wave::Wave(float decay_rate, float brightness_frequency, WaveMode mode)
        : decay_rate(decay_rate),
          brightness_frequency(brightness_frequency),
          mode(mode) {
    }

    void Wave::execute(Strip::Strip &strip, Iteration iteration) {
        time += 0.05f;
        color_time += 0.05f;

        uint16_t num_leds = strip.length();

        // Cosine-bouncing source position: oscillates between 0 and N-1 with
        // continuous velocity (no jolt at the bounce). Drives both the
        // distance-decay envelope and the hue-from-emission-time computation.
        float source_pos = (static_cast<float>(num_leds) - 1.0f) * 0.5f
                         * (1.0f - cosf(time * brightness_frequency * 2.0f * M_PI));

        // Subtle source brightness oscillation in [0.30, 1.00].
        float source_brightness = 0.65f + 0.35f * sinf(time * brightness_frequency * 2.0f * M_PI);

        float inv_num_leds = 1.0f / static_cast<float>(num_leds);
        // Wavefront propagation speed at the source, in pixels per second.
        // Used below to estimate when the wavefront currently at pixel i was
        // emitted, so its hue can be looked up from the time axis.
        float propagation_speed = static_cast<float>(num_leds) * brightness_frequency;

        for (uint16_t i = 0; i < num_leds; i++) {
            // Distance from the oscillating source: brightness peaks at the
            // source and decays symmetrically toward both ends. The envelope
            // is the only spatial modulation - the wavelength-based |sin(phase)|
            // layer that used to sit on top of it was removed to match the
            // reference implementation in scripts/build_pages.py.
            float distance = static_cast<float>(i) - source_pos;
            float abs_distance = fabsf(distance);
            float envelope = expf(-decay_rate * abs_distance * inv_num_leds);

            // Hue index based on the time at which the wavefront currently at
            // pixel i was emitted. The wavefront at pixel i was emitted about
            // |i - source_pos| / propagation_speed seconds ago.
            float emission_time = color_time - abs_distance / propagation_speed;
            Strip::Color pixel_color = Support::Color::wheel(emission_time * 20.0f);

            float final_brightness = source_brightness * envelope;

            uint8_t r = static_cast<uint8_t>(Support::Color::red(pixel_color) * final_brightness);
            uint8_t g = static_cast<uint8_t>(Support::Color::green(pixel_color) * final_brightness);
            uint8_t b = static_cast<uint8_t>(Support::Color::blue(pixel_color) * final_brightness);

            strip.setPixelColor(i, Support::Color::from_rgb(r, g, b));
        }
    }
} // namespace Show
