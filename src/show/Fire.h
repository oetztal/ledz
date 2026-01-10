#ifndef LEDZ_FIRE_H
#define LEDZ_FIRE_H
#include <random>
#include <functional>

#include <vector>
#include "Show.h"

namespace Show {
    class FireState {
        Strip::PixelIndex _length;
        Strip::PixelIndex start_offset;
        std::function<float()> randomFloat;
        float *temperature;

    public:
        FireState(std::function<float()> randomFloat,
                  Strip::PixelIndex length, Strip::PixelIndex start_offset = 0);

        virtual ~FireState();

        [[nodiscard]] Strip::PixelIndex length() const;

        void cooldown(float value) const;

        void spread(float spread_rate, float ignition, Strip::PixelIndex spark_range, float spark_amount,
                    const std::vector<float> &weights = {1.0f}, bool log = false);

        float get_temperature(Strip::PixelIndex pixel_index) const;
        void set_temperature(Strip::PixelIndex pixel_index, float value);
    };

    class Fire : public Show {
        std::unique_ptr<FireState> state;
        std::mt19937 gen;
        std::uniform_real_distribution<float> randomFloat;

        float cooling;
        float spread;
        float ignition;
        float spark_amount;
        std::vector<float> weights;

    public:
        Fire(float cooling = 1.0f, float spread = 1.0f, float ignition = 1.0f, float spark_amount = 0.5f,
             std::vector<float> weights = {1.0f});

        void ensureState(Strip::Strip &strip);

        void execute(Strip::Strip &strip, Iteration iteration) override;
    };
} // Show

#endif //LEDZ_FIRE_H
