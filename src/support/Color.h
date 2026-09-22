#ifndef LEDZ_SUPPORT_COLOR_H
#define LEDZ_SUPPORT_COLOR_H

#include "strip/Strip.h"

namespace Support::Color {
    Strip::Color wheel(float wheel_pos);

    Strip::Color from_rgb(Strip::ColorComponent red, Strip::ColorComponent green, Strip::ColorComponent blue);

    Strip::ColorComponent red(Strip::Color color);

    Strip::ColorComponent green(Strip::Color color);

    Strip::ColorComponent blue(Strip::Color color);

    Strip::Color black_body_color(float temperature);
}

#endif // LEDZ_SUPPORT_COLOR_H
