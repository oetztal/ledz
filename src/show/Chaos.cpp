//
// Created by Andreas W. on 02.01.26.
//

#include "Chaos.h"

#include "color.h"

namespace Show {
    float Chaos::func(float x) const {
        return r * x * (1 - x);
    }

    Chaos::Chaos() : Chaos(2.95f, 4.0f, 0.0002f) {
        // Delegate to parameterized constructor with defaults
    }

    Chaos::Chaos(float Rmin, float Rmax, float Rdelta)
        : Rmin(Rmin), Rmax(Rmax), Rdelta(Rdelta), r(Rmin) {
        // Initialize with provided parameters
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

        r += Rdelta;
        if (r > Rmax) {
            r = Rmin;
        }
    }
}
