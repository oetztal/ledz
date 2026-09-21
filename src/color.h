#ifndef LEDZ_COLOR_H
#define LEDZ_COLOR_H

#include "strip/Strip.h"

Strip::Color wheel(float wheel_pos);

Strip::Color color(Strip::ColorComponent red, Strip::ColorComponent green, Strip::ColorComponent blue);

// Extract color components from a Color value
Strip::ColorComponent red(Strip::Color color);

Strip::ColorComponent green(Strip::Color color);

Strip::ColorComponent blue(Strip::Color color);

#endif //LEDZ_COLOR_H