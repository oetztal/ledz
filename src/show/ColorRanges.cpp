#include "ColorRanges.h"
#include "../color.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace Show {
    ColorRanges::ColorRanges(const std::vector<Strip::Color> &colors,
                             const std::vector<float> &ranges,
                             const std::vector<BlendType> &blends)
        : colors(colors), ranges(ranges), blends(blends), initialized(false) {
    }

    void ColorRanges::execute(Strip::Strip &strip, Iteration iteration) {
        // Initialize color ranges on first run
        if (!initialized) {
#ifdef ARDUINO
            Serial.print("ColorRanges: colors=[");
            for (auto c: colors) {
                Serial.printf("RGB(%d,%d,%d), ", red(c), green(c), blue(c));
            }
            Serial.print("], ranges=[");
            for (auto range: ranges) {
                Serial.printf("%.1f%%, ", range);
            }
            Serial.print("], blends=[");
            for (auto b: blends) {
                Serial.printf("%s, ", b == BlendType::LINEAR ? "LINEAR" : "NONE");
            }
            Serial.println("]");
#endif

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

            // Validate blends if provided
            bool use_blends = !blends.empty();
            if (use_blends && blends.size() != colors.size() - 1) {
#ifdef ARDUINO
                Serial.printf(
                    "ColorRanges WARNING: Expected %zu blends for %zu colors, got %zu. Using NONE for all.\n",
                    colors.size() - 1, colors.size(), blends.size());
#endif
                use_blends = false;
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

            // Assign colors to each LED based on boundaries and blend types
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

                // Determine blend type for this transition
                BlendType blend_type = BlendType::NONE;
                if (use_blends && color_index < blends.size()) {
                    blend_type = blends[color_index];
                }

                Strip::Color result_color;
                if (blend_type == BlendType::LINEAR && color_index < colors.size() - 1) {
                    // Linear interpolation within this range
                    uint16_t range_start = boundaries[color_index];
                    uint16_t range_end = boundaries[color_index + 1];
                    uint16_t range_length = range_end - range_start;

                    // Calculate position ratio within the range (0.0 to 1.0)
                    float ratio = (range_length > 1)
                        ? (float)(led - range_start) / (float)(range_length - 1)
                        : 0.0f;

                    // Interpolate between current color and next color
                    Strip::Color colorA = colors[color_index];
                    Strip::Color colorB = colors[color_index + 1];

                    uint8_t r = (uint8_t)(red(colorA) * (1.0f - ratio) + red(colorB) * ratio);
                    uint8_t g = (uint8_t)(green(colorA) * (1.0f - ratio) + green(colorB) * ratio);
                    uint8_t b = (uint8_t)(blue(colorA) * (1.0f - ratio) + blue(colorB) * ratio);

                    result_color = color(r, g, b);
                } else {
                    // Solid color (NONE blend)
                    result_color = colors[color_index];
                }

                target_colors.push_back(result_color);
            }

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