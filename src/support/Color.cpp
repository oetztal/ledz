#include "Color.h"
#include <cmath>
#include <algorithm>

namespace Support::Color {

    Strip::Color wheel(float wheel_pos) {
        if (std::isnan(wheel_pos) || std::isinf(wheel_pos)) {
            return 0x000000; // Non-finite input → fail safe to black
        }

        wheel_pos = fmodf(wheel_pos, 255.0f);
        if (wheel_pos < 0.0f) wheel_pos += 255.0f;
        if (wheel_pos > 254.0f) wheel_pos = 254.0f; // safety edge for [254, 255) wrap

        // HSV cube-walking rainbow at full saturation/value. Walks the six faces
        // of the RGB cube (red -> yellow -> green -> cyan -> blue -> magenta ->
        // red), so complementary colours hit full intensity at the mid-sector
        // hues (yellow at h=42.5, cyan at h=127.5, magenta at h=212.5). Matches
        // the prior unsigned-char body byte-for-byte at every integer input
        // h ∈ [0, 254] (algebra: 255 * X / 255 == X for integer X < 2^24).
        float pos = wheel_pos * 6.0f / 255.0f;     // ∈ [0, ~5.976]
        auto section = static_cast<int>(pos);        // ∈ {0..5}
        float frac = pos - static_cast<float>(section); // ∈ [0, 1)

        switch (section) {
            case 0: // Red -> Yellow: R=255, G rises, B=0
                return (255u << 16) | (static_cast<unsigned>(255.0f * frac + 0.5f) << 8);
            case 1: // Yellow -> Green: R falls, G=255, B=0
                return (static_cast<unsigned>(255.0f * (1.0f - frac) + 0.5f) << 16) | (255u << 8);
            case 2: // Green -> Cyan: R=0, G=255, B rises
                return (255u << 8) | static_cast<unsigned>(255.0f * frac + 0.5f);
            case 3: // Cyan -> Blue: R=0, G falls, B=255
                return (static_cast<unsigned>(255.0f * (1.0f - frac) + 0.5f) << 8) | 255u;
            case 4: // Blue -> Magenta: R rises, G=0, B=255
                return (static_cast<unsigned>(255.0f * frac + 0.5f) << 16) | 255u;
            default: // Magenta -> Red: R=255, G=0, B falls (section 5)
                return (255u << 16) | static_cast<unsigned>(255.0f * (1.0f - frac) + 0.5f);
        }
    }

    Strip::Color from_rgb(Strip::ColorComponent red, Strip::ColorComponent green, Strip::ColorComponent blue) {
        return (red << 16) + (green << 8) + blue;
    }

    Strip::ColorComponent red(Strip::Color color) {
        return (color >> 16) & 0xFF;
    }

    Strip::ColorComponent green(Strip::Color color) {
        return (color >> 8) & 0xFF;
    }

    Strip::ColorComponent blue(Strip::Color color) {
        return color & 0xFF;
    }

    Strip::Color black_body_color(float temperature) {
        // Clamp temperature to valid range
        temperature = std::max(0.0f, std::min(1.0f, temperature));

        // Map normalized temperature (0-1) to Kelvin (800-1800K)
        // 0.0 → 800K (barely visible dull red)
        // 1.0 → 1800K (bright yellow)
        float kelvin = 800.0f + (temperature * 1000.0f);

        // Tanner Helland's algorithm
        // Temperature in Kelvin, divide by 100
        float temp = kelvin / 100.0f;

        // Red (always 255 for temps below 6600K - all wood fire temps)
        uint8_t red = 255;

        // Green: 99.4708025861 * ln(temp) - 161.1195681661
        float green_f = 99.4708025861f * std::log(temp) - 161.1195681661f;
        auto green = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, green_f)));

        // Blue: 138.5177312231 * ln(temp - 10) - 305.0447927307
        // (0 for temp <= 19, i.e., below 1900K)
        float blue_f = (temp <= 19.0f) ? 0.0f : 138.5177312231f * std::log(temp - 10.0f) - 305.0447927307f;
        auto blue = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, blue_f)));

        // Apply brightness scaling for low temperatures
        // At temp=0.0, brightness=0 (black)
        // At temp=0.3+, brightness=1.0 (full color)
        float brightness = std::min(1.0f, temperature / 0.3f);
        red = static_cast<uint8_t>(red * brightness);
        green = static_cast<uint8_t>(green * brightness);
        blue = static_cast<uint8_t>(blue * brightness);

        // Combine into a single color value
        return (static_cast<Strip::Color>(red) << 16) |
               (static_cast<Strip::Color>(green) << 8) |
               static_cast<Strip::Color>(blue);
    }
}