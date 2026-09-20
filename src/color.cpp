
#include "strip/Strip.h"

Strip::Color wheel(unsigned char wheel_pos) {
    if (wheel_pos > 254) {
        wheel_pos = 254; // Safeguard
    }

    // HSV cube-walking rainbow at full saturation/value. Walks the six faces
    // of the RGB cube (red -> yellow -> green -> cyan -> blue -> magenta ->
    // red), so complementary colours hit full intensity at their nominal hue
    // angle (yellow at pos~42, cyan at ~127, magenta at ~212). Replaces the
    // Adafruit edge-walking wheel, which only ever had one channel at full
    // intensity at any position and therefore could not produce pure yellow,
    // cyan, or magenta. Matches the reference in scripts/wave_show.py exactly
    // across all 255 positions (verified by exhaustive comparison).
    uint16_t pos = static_cast<uint16_t>(wheel_pos) * 6;

    if (wheel_pos <= 42) {
        // Red -> Yellow: R=255, G rises, B=0
        return (255u << 16) | (static_cast<unsigned>(pos) << 8);
    }
    if (wheel_pos <= 84) {
        // Yellow -> Green: R falls, G=255, B=0
        return (static_cast<unsigned>(510 - pos) << 16) | (255u << 8);
    }
    if (wheel_pos <= 127) {
        // Green -> Cyan: R=0, G=255, B rises
        return (255u << 8) | static_cast<unsigned>(pos - 510);
    }
    if (wheel_pos <= 169) {
        // Cyan -> Blue: R=0, G falls, B=255
        return (static_cast<unsigned>(1020 - pos) << 8) | 255u;
    }
    if (wheel_pos <= 212) {
        // Blue -> Magenta: R rises, G=0, B=255
        return (static_cast<unsigned>(pos - 1020) << 16) | 255u;
    }
    // Magenta -> Red: R=255, G=0, B falls
    return (255u << 16) | static_cast<unsigned>(1530 - pos);
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
