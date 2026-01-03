//
// Created by Andreas W. on 02.01.26.
//

#ifndef UNTITLED_CHAOS_H
#define UNTITLED_CHAOS_H
#include "Show.h"
#include "strip/Strip.h"

namespace Show {
    class Chaos : public Show {
        unsigned int iterations = 60;
        unsigned int color_factor = 4;
        const float x_initial = 0.5;
        const float r_start = 2.95;
        const float r_max = 4.0;
        const float r_incr = 0.0002;
        float r = r_start;

        float func(float x) const;
    public:
        Chaos();

        void execute(Strip::Strip &strip, Iteration iteration) override;
    };
}

#endif //UNTITLED_CHAOS_H
