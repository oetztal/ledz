#include "Fire.h"

#include <random>
#include <utility>

#include "color.h"
#include "support/color.h"

#ifdef ARDUINO
#include <USBCDC.h>
#endif

namespace Show {
    FireState::FireState(std::function<float()> randomFloat, Strip::PixelIndex length) : randomFloat(std::move(randomFloat)),
        _length(length) {
#ifdef ARDUINO
        Serial.printf("Fire::State::State %d\n", length);
#endif
        temperature = new float[length]{0.0f};
    }

    FireState::~FireState() {
        delete[] temperature;
    }

    Strip::PixelIndex FireState::length() const {
        return this->_length;
    }

    void FireState::cooldown(float value) const {
        for (int i = 0; i < length(); i++) {
            temperature[i] = std::max(0.0f, temperature[i] - value);
        }
    }

    void FireState::spread(float spread_rate, float ignition, Strip::PixelIndex spark_range, bool log) {
        float previous = 0.0f;
        for (int i = 0; i < length(); i++) {
            auto spreadValue = std::min(0.25f, previous) * spread_rate * randomFloat();
            auto spread = std::min(previous, spreadValue);
#ifdef ARDUINO
            if (i < 10 && log) {
                Serial.printf("spread %d %f %f (%f)\n", i, temperature[i], spread, previous);
            }
#else
            printf("spread %d %f %f (%f)\n", i, temperature[i], spread, previous);
#endif
            temperature[i] += spread;
            if (i > 0) {
                temperature[i - 1] -= spread;
            }

            if (i < spark_range && randomFloat() <= ignition) {
                temperature[i] += 2.0;
            }
            previous = temperature[i];
        }
    }


    Fire::Fire(float cooling, float spread, float ignition) : cooling(cooling), spread(spread), ignition(ignition),
                                                              randomFloat(0.0f, 1.0f) {
        std::random_device rd;
        this->gen = std::mt19937(rd());
    }

    void Fire::ensureState(Strip::Strip &strip) {
        if (!state || state->length() != strip.length()) {
            state = std::make_unique<FireState>([=] { return randomFloat(gen); }, strip.length());
        }
    }

    void Fire::execute(Strip::Strip &strip, Iteration iteration) {
        ensureState(strip);

        state->cooldown(this->cooling * randomFloat(gen));

        state->spread(this->spread, this->ignition, 5,iteration % 100 == 0);

        for (Strip::PixelIndex i = 0; i < strip.length(); i++) {
            strip.setPixelColor(i, Support::Color::black_body_color(state->temperature[i]));
        }
    }
} // Show
