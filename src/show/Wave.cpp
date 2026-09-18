#include "Wave.h"
#include "../color.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Show {
    Wave::Wave(float decay_rate, float brightness_frequency, float wavelength, WaveMode mode)
        : decay_rate(decay_rate),
          brightness_frequency(brightness_frequency),
          wavelength(wavelength),
          mode(mode),
          time(0.0f), color_time(0.0f) {
    }

    void Wave::execute(Strip::Strip &strip, Iteration iteration) {
        time += 0.05f;
        color_time += 0.05f;

        uint16_t num_leds = strip.length();

        // Cosine-bouncing source position: oscillates between 0 and N-1 with
        // continuous velocity (no jolt at the bounce). Used only for the
        // brightness envelope and the hue-from-emission-time computation; the
        // phase itself is mode-aware below.
        float source_pos = (static_cast<float>(num_leds) - 1.0f) * 0.5f
                         * (1.0f - cosf(time * brightness_frequency * 2.0f * M_PI));

        // Subtle source brightness oscillation (kept from the original show).
        float source_brightness = 0.65f + 0.35f * sinf(time * brightness_frequency * 2.0f * M_PI);

        float inv_wavelength = 1.0f / wavelength;
        float inv_num_leds = 1.0f / static_cast<float>(num_leds);
        // Traveling mode adds a uniform time term to every pixel's phase.
        // Hoisted out of the loop so it costs one multiply per frame, not per
        // pixel.
        float traveling_time_term = (mode == WaveMode::Traveling)
                                  ? time * brightness_frequency * 2.0f * M_PI
                                  : 0.0f;

        for (uint16_t i = 0; i < num_leds; i++) {
            // Distance from the oscillating source: brightness peaks at the
            // source and decays symmetrically toward both ends. The envelope
            // is identical in both modes; only the wave phase below differs.
            float distance = static_cast<float>(i) - source_pos;
            float abs_distance = fabsf(distance);
            float envelope = expf(-decay_rate * abs_distance * inv_num_leds);

            // Mode-aware phase. Bounce: stripes phase-lock to the source and
            // move with it. Traveling: stripes drift across the strip at
            // `freq * wavelength` pixels per second, independent of source
            // motion. The hue-from-emission-time formula below still uses the
            // bouncing source, so the rainbow trail feels coherent in both.
            float phase;
            if (mode == WaveMode::Traveling) {
                phase = (static_cast<float>(i) * inv_wavelength * 2.0f * M_PI)
                      - traveling_time_term;
            } else {
                phase = distance * 2.0f * M_PI * inv_wavelength;
            }
            float wave_brightness = fabsf(sinf(phase));

            // Hue index based on the time at which the wavefront currently at
            // pixel i was emitted. For a source moving at roughly N * freq
            // pixels per second, the wavefront at pixel i was emitted about
            // |i - source_pos| / (N * freq) seconds ago.
            float propagation_speed = static_cast<float>(num_leds) * brightness_frequency;
            float emission_time = color_time - abs_distance / propagation_speed;
            uint8_t color_index = static_cast<uint8_t>(static_cast<int>(emission_time * 20.0f) % 255);
            Strip::Color pixel_color = wheel(color_index);

            float final_brightness = source_brightness * wave_brightness * envelope;

            uint8_t r = static_cast<uint8_t>(red(pixel_color) * final_brightness);
            uint8_t g = static_cast<uint8_t>(green(pixel_color) * final_brightness);
            uint8_t b = static_cast<uint8_t>(blue(pixel_color) * final_brightness);

            strip.setPixelColor(i, color(r, g, b));
        }
    }
} // namespace Show