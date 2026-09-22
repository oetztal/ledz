#include "Starlight.h"
#include "../support/Color.h"

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <cstdlib>  // For rand(), RAND_MAX
#endif

namespace {
    float randomChance() {
#ifdef ARDUINO
        return (float) random(1000) / 1000.0f;
#else
        return (float) rand() / (float) RAND_MAX;
#endif
    }
} // namespace

namespace Show {
    Starlight::Starlight(float probability, unsigned long length_ms, unsigned long fade_ms,
                         uint8_t r, uint8_t g, uint8_t b)
        : probability(probability), length_ms(length_ms), fade_ms(fade_ms),
          star_color(Support::Color::from_rgb(r, g, b)) {
    }

    float Starlight::calculateBrightness(unsigned long elapsed_ms) {
        // Phase 1: Fade-in (0 to fade_ms)
        if (elapsed_ms < fade_ms) {
            return (float) elapsed_ms / (float) fade_ms;
        }

        // Phase 2: Hold at full brightness (fade_ms to fade_ms + length_ms)
        unsigned long hold_end = fade_ms + length_ms;
        if (elapsed_ms < hold_end) {
            return 1.0f;
        }

        // Phase 3: Fade-out (hold_end to hold_end + fade_ms)
        if (unsigned long fade_out_start = hold_end;
            elapsed_ms < fade_out_start + fade_ms) {
            unsigned long fade_out_elapsed = elapsed_ms - fade_out_start;
            return 1.0f - ((float) fade_out_elapsed / (float) fade_ms);
        }

        // Star has completed its lifecycle
        return 0.0f;
    }

    void Starlight::execute(Strip::Strip &strip, Iteration iteration) {
#ifdef ARDUINO
        unsigned long current_time = millis();
#else
        // For native builds, use iteration count as time proxy (10ms per iteration)
        unsigned long current_time = iteration * 10;
#endif
        uint16_t num_leds = strip.length();

        // Spawn new stars based on probability
        // Use a random float between 0.0 and 1.0
        if (float spawn_chance = randomChance(); spawn_chance < probability) {
            // Pick a random LED that's not already an active star
#ifdef ARDUINO
            uint16_t led = static_cast<uint16_t>(random(num_leds));
#else
            uint16_t led = static_cast<uint16_t>(rand() % num_leds);
#endif
            if (active_stars.find(led) == active_stars.end()) {
                active_stars[led] = current_time;
            }
        }

        // Clear the strip
        for (uint16_t i = 0; i < num_leds; i++) {
            strip.setPixelColor(i, Support::Color::from_rgb(0, 0, 0));
        }

        // Update and render all active stars
        auto it = active_stars.begin();
        while (it != active_stars.end()) {
            uint16_t led = it->first;
            unsigned long start_time = it->second;
            unsigned long elapsed = current_time - start_time;

            // Check if star has expired
            if (elapsed >= getTotalLifetime()) {
                it = active_stars.erase(it);
                continue;
            }

            // Calculate brightness and apply to LED
            float brightness = calculateBrightness(elapsed);
            auto r = (uint8_t)(Support::Color::red(star_color) * brightness);
            auto g = (uint8_t)(Support::Color::green(star_color) * brightness);
            auto b = (uint8_t)(Support::Color::blue(star_color) * brightness);

            strip.setPixelColor(led, Support::Color::from_rgb(r, g, b));
            ++it;
        }
    }
} // namespace Show