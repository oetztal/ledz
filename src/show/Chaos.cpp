//
// Created by Andreas W. on 02.01.26.
//

#include "Chaos.h"

#include "color.h"

namespace Show {
    float Chaos::func(float x) const {
        return r * x * (1 - x);
    }

    Chaos::Chaos() {
    }

    void Chaos::execute(Strip::Strip &strip, Iteration iteration) {
        strip.fill(0x000000);

        auto num_leds = strip.length();

        auto x = x_initial;
        for (int i; i< iterations; i++) {
            x = func(x);

            Strip::PixelIndex led = int(x * (num_leds - 1));
            Strip::Color color = wheel((i * color_factor) % 255);
            strip.setPixelColor(led, color);
        }

        r += r_incr;
        if (r > r_max) {
            r = r_start;
        }
    }
}
