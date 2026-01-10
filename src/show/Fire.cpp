#include "Fire.h"

#include <random>
#include <utility>

#include "color.h"
#include "support/color.h"

#ifdef ARDUINO
#include <USBCDC.h>
#endif

namespace Show {
    FireState::FireState(std::function<float()> randomFloat, Strip::PixelIndex length,
                         Strip::PixelIndex start_offset) : randomFloat(std::move(randomFloat)),
                                                           _length(length), start_offset(start_offset) {
#ifdef ARDUINO
        Serial.printf("Fire::State::State %d\n", length);
#endif
        temperature = new float[length + start_offset]{0.0f};
    }

    FireState::~FireState() {
        delete[] temperature;
    }

    Strip::PixelIndex FireState::length() const {
        return this->_length;
    }

    void FireState::cooldown(float value) const {
        for (int i = 0; i < length(); i++) {
            temperature[i + start_offset] = std::max(0.0f, temperature[i + start_offset] - value);
        }
    }

    void FireState::spread(float spread_rate, float ignition, Strip::PixelIndex spark_range, float spark_amount,
                           const std::vector<float> &weights, bool log) {
        for (int i = 0; i < length(); i++) {
            float weighted_previous = 0.0f;
            float available_energy = 0.0f;
            float local_total_weight = 0.0f;

            for (size_t w_idx = 0; w_idx < weights.size(); ++w_idx) {
                int prev_idx = i - 1 - (int) w_idx;
                if (prev_idx >= 0) {
                    local_total_weight += weights[w_idx];
                }
            }

            if (local_total_weight > 0) {
                for (size_t w_idx = 0; w_idx < weights.size(); ++w_idx) {
                    int prev_idx = i - 1 - (int) w_idx;
                    if (prev_idx >= 0) {
                        float w = weights[w_idx] / local_total_weight;
                        weighted_previous += temperature[prev_idx + start_offset] * w;
                        available_energy += temperature[prev_idx + start_offset];
                    }
                }
            }

            auto spreadValue = std::min(0.25f, weighted_previous) * spread_rate * randomFloat();
            // We can't take more than what's available in any of the contributing pixels if we want to be safe,
            // but the weighted_previous already gives us a good limit.
            // To ensure energy conservation, we must ensure spread <= sum of contributing temperatures.
            auto spread = std::min(available_energy, spreadValue);

            if (spread > 0) {
                temperature[i + start_offset] += spread;
                for (size_t w_idx = 0; w_idx < weights.size(); ++w_idx) {
                    int prev_idx = i - 1 - (int) w_idx;
                    if (prev_idx >= 0) {
                        float w = weights[w_idx] / local_total_weight;
                        temperature[prev_idx + start_offset] -= spread * w;
                    }
                }
            }

            if (i < spark_range && randomFloat() <= ignition) {
                temperature[i + start_offset] += spark_amount;
            }
        }
    }

    float FireState::get_temperature(Strip::PixelIndex pixel_index) const {
        return temperature[pixel_index + start_offset];
    }

    void FireState::set_temperature(Strip::PixelIndex pixel_index, float value) {
        temperature[pixel_index + start_offset] = value;
    }


    Fire::Fire(float cooling, float spread, float ignition, float spark_amount, std::vector<float> weights) :
        cooling(cooling),
        spread(spread), ignition(ignition), spark_amount(spark_amount),
        weights(std::move(weights)),
        randomFloat(0.0f, 1.0f) {
        std::random_device rd;
        this->gen = std::mt19937(rd());
    }

    void Fire::ensureState(Strip::Strip &strip) {
        if (!state || state->length() != strip.length()) {
            state = std::make_unique<FireState>([=] { return randomFloat(gen); }, strip.length(), 5);
        }
    }

    void Fire::execute(Strip::Strip &strip, Iteration iteration) {
        ensureState(strip);

        state->cooldown(this->cooling * randomFloat(gen));

        state->spread(this->spread, this->ignition, 2, this->spark_amount, this->weights, iteration % 100 == 0);

        for (Strip::PixelIndex i = 0; i < strip.length(); i++) {
            strip.setPixelColor(i, Support::Color::black_body_color(state->get_temperature(i)));
        }
    }
} // Show
