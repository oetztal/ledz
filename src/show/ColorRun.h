#ifndef LEDZ_COLORRUN_H
#define LEDZ_COLORRUN_H

#include <vector>
#include <random>

#include "strip/Strip.h"
#include "Show.h"
#include "support/Random.h"

namespace Show {
    class ColorRun : public Show {
        class State {
            Iteration start;
            float speed;
            Strip::Color color;

        public:
            State(Iteration start, float speed, Strip::Color color) : start(start), speed(speed), color(color) {}

            [[nodiscard]] Strip::PixelIndex position(Iteration iteration) const;

            [[nodiscard]] Strip::Color getColor() const;
        };

        std::uniform_int_distribution<> randomPercent{0, 99};
        std::uniform_int_distribution<> randomPhase{0, 7};
        std::uniform_int_distribution<> randomSpeed{20, 60};

    public:
        ColorRun();

        void update_state(Iteration iteration);

        void clean_up_state(Strip::PixelIndex length, Iteration iteration);

        void execute(Strip::Strip& strip, Iteration iteration) override;

    private:
        std::vector<Strip::Color> phases = {0x000000, 0x0000FF, 0x00FF00, 0x00FFFF,
                                            0xFF0000, 0xFF00FF, 0xFFFF00, 0xFFFFFF};
        std::vector<State> states = {State{0, 0.5, 0xFF0000}};
        Support::Random gen;
    };
} // Show

#endif // LEDZ_COLORRUN_H
