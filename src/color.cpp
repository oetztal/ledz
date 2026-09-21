
#include <cmath>
#include "strip/Strip.h"

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
    int section = static_cast<int>(pos);        // ∈ {0..5}
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

Strip::Color color(Strip::ColorComponent red, Strip::ColorComponent green, Strip::ColorComponent blue) {
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
