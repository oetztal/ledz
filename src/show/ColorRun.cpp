#include "ColorRun.h"

#include <algorithm>
#include <sstream>
#ifdef ARDUINO
#include <USBCDC.h>
#endif

namespace Show {
    ColorRun::ColorRun() {
        gen.seed(Support::randomSeed());
    }

    void ColorRun::update_state(Iteration iteration) {
        if (randomPercent(gen) >= 95) {
            auto speed = static_cast<float>(randomSpeed(gen)) / 100.0f;
            auto color = phases[randomPhase(gen)];
            auto state = State{iteration, speed, color};
            states.push_back(state);
        }
    }

    void ColorRun::execute(Strip::Strip& strip, Iteration iteration) {
        update_state(iteration);

        strip.fill(0x000000);

        for (auto state : states) {
            strip.setPixelColor(state.position(iteration), state.getColor());
        }

        clean_up_state(strip.length(), iteration);
    }

    void ColorRun::clean_up_state(Strip::PixelIndex length, Iteration iteration) {
        states.erase(std::remove_if(states.begin(), states.end(),
                                    [&](const State& state) { return state.position(iteration) >= length; }),
                     states.end());
    }

    Strip::PixelIndex ColorRun::State::position(Iteration iteration) const {
        return static_cast<Strip::PixelIndex>(speed * static_cast<float>(iteration - start));
    }

    Strip::Color ColorRun::State::getColor() const {
        return color;
    }
} // Show
