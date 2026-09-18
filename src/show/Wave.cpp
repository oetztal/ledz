#include "Wave.h"
#include "../color.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Show {
    Wave::Wave(float decay_rate, float brightness_frequency, float wavelength)
        : decay_rate(decay_rate),
          brightness_frequency(brightness_frequency),
          wavelength(wavelength),
          time(0.0f), color_time(0.0f) {
    }

    void Wave::execute(Strip::Strip &strip, Iteration iteration) {
        time += 0.05f;
        color_time += 0.05f;

        uint16_t num_leds = strip.length();

        // Cosine-bouncing source position: oscillates between 0 and N-1 with
        // continuous velocity (no jolt at the bounce). Velocity is zero at the
        // extremes, so the source lingers there momentarily; the end-fade below
        // hides that hot spot.
        float t = time * brightness_frequency * 2.0f * M_PI;
        float source_pos = (static_cast<float>(num_leds) - 1.0f) * 0.5f
                         * (1.0f - cosf(t));

        // End-fade factor: full brightness when the source is mid-strip, zero
        // at the ends. Counters the cosine source lingering at the extremes.
        float pos_norm = source_pos / static_cast<float>(num_leds - 1);
        float end_fade = 1.0f - 2.0f * fabsf(pos_norm - 0.5f);

        // Subtle source brightness oscillation, scaled by the end-fade.
        float source_brightness = end_fade * (0.65f + 0.35f * sinf(t));

        float inv_wavelength = 1.0f / wavelength;

        for (uint16_t i = 0; i < num_leds; i++) {
            // Distance from the oscillating source: brightness peaks at the
            // source and decays symmetrically toward both ends. Decay is
            // measured in units of wavelength so the falloff is independent of
            // strip length: the same `decay_rate` produces the same visual
            // decay on a 30-pixel strip as on a 144-pixel strip.
            float distance = static_cast<float>(i) - source_pos;
            float abs_distance = fabsf(distance);
            float envelope = expf(-decay_rate * abs_distance * inv_wavelength);

            // Signed sine from the source position; take the absolute value so
            // the strip stays positive-valued and lit. Future interference
            // work can sum multiple sources as `|wave_a + wave_b|`.
            float wave = sinf(distance * 2.0f * M_PI * inv_wavelength);
            float wave_brightness = fabsf(wave);

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