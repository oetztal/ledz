//
// Created by Andreas W. on 02.01.26.
//

#ifndef UNTITLED_COLOR_H
#define UNTITLED_COLOR_H

#include "strip/Strip.h"

Strip::Color wheel(unsigned char wheel_pos);

Strip::Color color(Strip::ColorComponent red, Strip::ColorComponent green, Strip::ColorComponent blue);

#endif //UNTITLED_COLOR_H