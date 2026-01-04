//
// Created by Claude Code on 03.01.26.
//

#include "TwoColorBlend.h"
#include "../color.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace Show {

    TwoColorBlend::TwoColorBlend(uint8_t r1, uint8_t g1, uint8_t b1,
                                 uint8_t r2, uint8_t g2, uint8_t b2)
        : color1(color(r1, g1, b1)), color2(color(r2, g2, b2)),
          gradient_initialized(false) {
    }

    void TwoColorBlend::execute(Strip::Strip &strip, Iteration iteration) {
        // Initialize gradient on first run
        if (!gradient_initialized) {
            uint16_t num_leds = strip.length();
            std::vector<Strip::Color> target_colors;
            target_colors.reserve(num_leds);

            // Calculate gradient for each LED position
            for (uint16_t i = 0; i < num_leds; i++) {
                // Calculate blend factor (0.0 at start, 1.0 at end)
                float blend_factor = (float)i / (float)(num_leds - 1);

                // Linear interpolation between color1 and color2
                uint8_t r = (uint8_t)(red(color1) * (1.0f - blend_factor) + red(color2) * blend_factor);
                uint8_t g = (uint8_t)(green(color1) * (1.0f - blend_factor) + green(color2) * blend_factor);
                uint8_t b = (uint8_t)(blue(color1) * (1.0f - blend_factor) + blue(color2) * blend_factor);

                target_colors.push_back(color(r, g, b));
            }

#ifdef ARDUINO
            Serial.println("TwoColorBlend: Creating gradient blend");
#endif

            // Create SmoothBlend with the gradient target colors
            blend = std::make_unique<Support::SmoothBlend>(strip, target_colors);
            gradient_initialized = true;
        }

        // Step through the smooth blend transition
        if (blend != nullptr && !blend->isComplete()) {
            blend->step();
        }
    }

} // namespace Show
