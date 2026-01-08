#ifndef LEDZ_FIRE_H
#define LEDZ_FIRE_H
#include <random>
#include <functional>

#include "Show.h"

namespace Show {
    class FireState {
        Strip::PixelIndex _length;
        std::function<float()> randomFloat;

    public:
        FireState(std::function<float()> randomFloat,
                  Strip::PixelIndex length);

        virtual ~FireState();

        [[nodiscard]] Strip::PixelIndex length() const;

        void cooldown(float value) const;

        void spread(float spread_rate, float ignition, Strip::PixelIndex spark_range, bool log = false);

        float *temperature;
    };

    class Fire : public Show {

        std::unique_ptr<FireState> state;
        std::mt19937 gen;
        std::uniform_real_distribution<float> randomFloat;

        float cooling;
        float spread;
        float ignition;

    public:
        Fire(float cooling=1.0f, float spread=1.0f, float ignition=1.0f);

        void ensureState(Strip::Strip &strip);

        void execute(Strip::Strip &strip, Iteration iteration) override;
    };
} // Show

#endif //LEDZ_FIRE_H
