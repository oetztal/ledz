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

    void FireState::spread(float spread_rate, float ignition, Strip::PixelIndex spark_range, const std::vector<float>& weights, bool log) {
        for (int i = 0; i < length(); i++) {
            float weighted_previous = 0.0f;
            float available_energy = 0.0f;
            float local_total_weight = 0.0f;

            for (size_t w_idx = 0; w_idx < weights.size(); ++w_idx) {
                int prev_idx = i - 1 - (int)w_idx;
                if (prev_idx >= 0) {
                    local_total_weight += weights[w_idx];
                }
            }

            if (local_total_weight > 0) {
                for (size_t w_idx = 0; w_idx < weights.size(); ++w_idx) {
                    int prev_idx = i - 1 - (int)w_idx;
                    if (prev_idx >= 0) {
                        float w = weights[w_idx] / local_total_weight;
                        weighted_previous += temperature[prev_idx] * w;
                        available_energy += temperature[prev_idx];
                    }
                }
            }

            auto spreadValue = std::min(0.25f, weighted_previous) * spread_rate * randomFloat();
            // We can't take more than what's available in any of the contributing pixels if we want to be safe,
            // but the weighted_previous already gives us a good limit.
            // To ensure energy conservation, we must ensure spread <= sum of contributing temperatures.
            auto spread = std::min(available_energy, spreadValue);

            if (spread > 0) {
                temperature[i] += spread;
                for (size_t w_idx = 0; w_idx < weights.size(); ++w_idx) {
                    int prev_idx = i - 1 - (int)w_idx;
                    if (prev_idx >= 0) {
                        float w = weights[w_idx] / local_total_weight;
                        temperature[prev_idx] -= spread * w;
                    }
                }
            }

            if (i < spark_range && randomFloat() <= ignition) {
                temperature[i] += 1.0;
            }
        }
    }


    Fire::Fire(float cooling, float spread, float ignition, std::vector<float> weights) : cooling(cooling), spread(spread), ignition(ignition),
                                                              weights(std::move(weights)),
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

        state->spread(this->spread, this->ignition, 2, this->weights, iteration % 100 == 0);

        for (Strip::PixelIndex i = 0; i < strip.length(); i++) {
            strip.setPixelColor(i, Support::Color::black_body_color(state->temperature[i]));
        }
    }
} // Show
