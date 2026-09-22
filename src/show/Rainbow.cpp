#include "../support/Color.h"
#include "Rainbow.h"

namespace Show {
    Rainbow::Rainbow(float time_step, float pixel_step) : time_step(time_step), pixel_step(pixel_step) {}

    void Rainbow::execute(Strip::Strip& strip, Iteration iteration) {
        for (Strip::PixelIndex index = 0; index < strip.length(); index++) {
            float hue_position = static_cast<float>(iteration) * time_step + static_cast<float>(index) * pixel_step;

            strip.setPixelColor(index, Support::Color::wheel(hue_position));
        }
    }
}
