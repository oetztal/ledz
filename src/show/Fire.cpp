#include "Fire.h"

#include <algorithm>
#include <random>
#include <utility>

#include "support/color.h"

#ifdef ARDUINO
#include <USBCDC.h>
#endif

namespace Show {
    FireState::FireState(std::function<float()> randomFloat, Strip::PixelIndex length) :
        randomFloat(std::move(randomFloat)),
        _length(length),
        temperature(std::make_unique<float[]>(length)),
        prev_temperature(std::make_unique<float[]>(length)) {
#ifdef ARDUINO
        Serial.printf("Fire::State::State %d\n", length);
#endif
        std::fill(temperature.get(), temperature.get() + length, 0.0f);
        std::fill(prev_temperature.get(), prev_temperature.get() + length, 0.0f);
    }

    Strip::PixelIndex FireState::length() const {
        return this->_length;
    }

    void FireState::cooldown(float value) const {
        for (int i = 0; i < length(); i++) {
            temperature[i] = std::max(0.0f, temperature[i] - value);
        }
    }

    void FireState::spread(float spread_rate, float ignition, Strip::PixelIndex spark_range, float spark_amount,
                           const std::vector<float> &weights, bool log) {
        // Copy current state to previous buffer for consistent reads during this frame
        std::copy(temperature.get(), temperature.get() + length(), prev_temperature.get());

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
                        // Read from previous frame snapshot
                        weighted_previous += prev_temperature[prev_idx] * w;
                        available_energy += prev_temperature[prev_idx];
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
                    int prev_idx = i - 1 - (int) w_idx;
                    if (prev_idx >= 0) {
                        float w = weights[w_idx] / local_total_weight;
                        temperature[prev_idx] -= spread * w;
                    }
                }
            }

            if (i < spark_range && randomFloat() <= ignition) {
                temperature[i] += spark_amount;
            }
        }
    }

    float FireState::get_temperature(Strip::PixelIndex pixel_index) const {
        if (pixel_index < 0 || pixel_index >= length()) {
            return 0.0f;
        }
        return temperature[pixel_index];
    }

    void FireState::set_temperature(Strip::PixelIndex pixel_index, float value) {
        if (pixel_index >= 0 && pixel_index < length()) {
            temperature[pixel_index] = value;
        }
    }


    Fire::Fire(float cooling, float spread, float ignition, float spark_amount, std::vector<float> weights, Strip::PixelIndex start_offset) :
        cooling(cooling),
        spread(spread), ignition(ignition), spark_amount(spark_amount),
        weights(std::move(weights)),
        start_offset(start_offset),
        randomFloat(0.0f, 1.0f) {
        std::random_device rd;
        this->gen = std::mt19937(rd());
    }

    void Fire::ensureState(Strip::Strip &strip) {
        if (!state || state->length() != strip.length() + start_offset) {
            state = std::make_unique<FireState>([=] { return randomFloat(gen); }, strip.length() + start_offset);
        }
    }

    void Fire::execute(Strip::Strip &strip, Iteration iteration) {
        ensureState(strip);

        state->cooldown(this->cooling * randomFloat(gen));

        state->spread(this->spread, this->ignition, 2, this->spark_amount, this->weights, iteration % 100 == 0);

        for (Strip::PixelIndex i = 0; i < strip.length(); i++) {
            // Mapping strip index i to state index i + visual_offset
            strip.setPixelColor(i, Support::Color::black_body_color(state->get_temperature(i + start_offset)));
        }
    }
} // Show
