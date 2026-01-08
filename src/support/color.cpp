#include "color.h"

namespace Support::Color {
    Strip::Color black_body_color(float temperature) {
        Strip::Color color;
        if (temperature <= 0.333333f) {
            color = static_cast<Strip::Color>(255 * temperature / 0.33333f) << 16;
        } else if (temperature <= 0.66666f) {
            auto component = static_cast<Strip::Color>(255 * (temperature - 0.33333f) / 0.33333f);
            color = (255 << 16) + (component << 8);
        } else {
            temperature = std::min(temperature, 1.0f);
            auto component = static_cast<Strip::Color>(255 * (temperature - 0.66666f) / 0.33333f);
            color = (255 << 16) + (255 << 8 ) + component;
        }
        return color;
    }
}
