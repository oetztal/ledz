//
// Created by Andreas W. on 02.01.26.
//

#ifndef UNTITLED_COLORRUN_H
#define UNTITLED_COLORRUN_H

#include <vector>
#include <random>

#include "strip/Strip.h"
#include "Show.h"

namespace Show {
    class ColorRun : public Show {
        class State {
            Iteration start;
            float speed;

        public:
            Strip::Color color;
            State(Iteration start, float speed, Strip::Color color)
                : start(start),
                  speed(speed),
                  color(color) {
            }


            Strip::PixelIndex position(Iteration iteration) const;
        };
        std::uniform_int_distribution<> randomPercent;
        std::uniform_int_distribution<> randomPhase; // (0, phases.size() - 1);
        std::uniform_int_distribution<> randomSpeed; //(20, 60);

    public:
        ColorRun();

        void update_state(Iteration iteration);

        void clean_up_state(Strip::PixelIndex length, Iteration iteration);

        void execute(Strip::Strip &strip, Iteration iteration) override;

    private:
        std::vector<Strip::Color> phases;
        std::vector<State> states;
        std::mt19937 gen;
    };
} // Show

#endif //UNTITLED_COLORRUN_H
