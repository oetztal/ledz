//
// Created by Claude Code on 03.01.26.
//

#include "ColorRanges.h"
#include "../color.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace Show {
    ColorRanges::ColorRanges(const std::vector<Strip::Color> &colors, const std::vector<float> &ranges)
        : colors(colors), ranges(ranges), initialized(false) {
    }

    void ColorRanges::execute(Strip::Strip &strip, Iteration iteration) {
        // Initialize color ranges on first run
        if (!initialized) {
            uint16_t num_leds = strip.length();
            std::vector<Strip::Color> target_colors;
            target_colors.reserve(num_leds);

            // Build boundary indices
            std::vector<uint16_t> boundaries;
            boundaries.push_back(0); // Always start at 0

            // Validate ranges if provided
            bool use_custom_ranges = !ranges.empty();
            if (use_custom_ranges && ranges.size() != colors.size() - 1) {
#ifdef ARDUINO
                Serial.printf(
                    "ColorRanges WARNING: Expected %zu ranges for %zu colors, got %zu. Using equal distribution.\n",
                    colors.size() - 1, colors.size(), ranges.size());
#endif
                use_custom_ranges = false;
            }

            if (!use_custom_ranges) {
                // Equal distribution
#ifdef ARDUINO
                Serial.printf("ColorRanges: Using equal distribution for %zu colors\n", colors.size());
#endif
                for (size_t i = 1; i < colors.size(); i++) {
                    uint16_t boundary = (uint16_t)((float) num_leds * i / colors.size());
                    boundaries.push_back(boundary);
                }
            } else {
                // Custom ranges (percentages)
#ifdef ARDUINO
                Serial.printf("ColorRanges: Using custom ranges for %zu colors: ", colors.size());
                for (size_t i = 0; i < ranges.size(); i++) {
                    Serial.printf("%.1f%% ", ranges[i]);
                }
                Serial.println();
#endif
                for (float range: ranges) {
                    uint16_t boundary = (uint16_t)((float) num_leds * range / 100.0f);
                    boundaries.push_back(boundary);
                }
            }
            boundaries.push_back(num_leds); // Always end at num_leds

#ifdef ARDUINO
            Serial.printf("ColorRanges: Final boundaries (%zu total):\n", boundaries.size());
            for (size_t i = 0; i < boundaries.size(); i++) {
                Serial.printf("  [%zu] LED %u", i, boundaries[i]);
                if (i > 0 && i <= colors.size()) {
                    uint8_t r = red(colors[i - 1]);
                    uint8_t g = green(colors[i - 1]);
                    uint8_t b = blue(colors[i - 1]);
                    Serial.printf(" (Color %zu: RGB(%d,%d,%d))", i - 1, r, g, b);
                }
                Serial.println();
            }
#endif

            // Assign colors to each LED based on boundaries
            for (uint16_t led = 0; led < num_leds; led++) {
                // Find which color range this LED belongs to
                size_t color_index = 0;
                for (size_t i = 0; i < boundaries.size() - 1; i++) {
                    if (led >= boundaries[i] && led < boundaries[i + 1]) {
                        color_index = i;
                        break;
                    }
                }

                // Make sure we don't go out of bounds
                if (color_index >= colors.size()) {
                    color_index = colors.size() - 1;
                }

                target_colors.push_back(colors[color_index]);
            }

#ifdef ARDUINO
            Serial.println("ColorRanges: Creating smooth blend");
#endif

            // Create SmoothBlend with the color range pattern
            blend = std::make_unique<Support::SmoothBlend>(strip, target_colors);
            initialized = true;
        }

        // Step through the smooth blend transition
        if (blend != nullptr && !blend->isComplete()) {
            blend->step();
        }
    }
} // namespace Show