#include "color.h"

namespace Support::Color {
    const float BODY_COLOR_1 = 0.30f;
    const float BODY_COLOR_1_WIDTH = BODY_COLOR_1;
    const float BODY_COLOR_2 = 1.00f;
    const float BODY_COLOR_2_WIDTH = BODY_COLOR_2 - BODY_COLOR_1;
    const float BODY_COLOR_3_WIDTH = 1.0f - BODY_COLOR_2;

    Strip::Color black_body_color(float temperature) {
        Strip::Color color;
        if (temperature <= BODY_COLOR_1) {
            color = static_cast<Strip::Color>(255 * temperature / BODY_COLOR_1_WIDTH) << 16;
            // } else if (temperature <= 0.66666f) {
            //     auto component = static_cast<Strip::Color>(255 * (temperature - BODY_COLOR_1) / BODY_COLOR_2_WIDTH);
            //     color = (255 << 16) + (component << 8);
        } else {
            temperature = std::min(temperature, 1.0f);
            // auto component = static_cast<Strip::Color>(255 * (temperature - BODY_COLOR_2) / BODY_COLOR_3_WIDTH);
            auto component = static_cast<Strip::Color>(255 * (temperature - BODY_COLOR_1) / BODY_COLOR_2_WIDTH);
            color = (255 << 16) + (component << 8);
            // color = (255 << 16) + (255 << 8 ) + component;
        }
        return color;
    }
}
