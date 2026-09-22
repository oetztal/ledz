#ifndef LEDZ_RAINBOW_H
#define LEDZ_RAINBOW_H
#include "Show.h"
#include "strip/Strip.h"

namespace Show {
    class Rainbow : public Show {
    private:
        float time_step;
        float pixel_step;

    public:
        Rainbow(float time_step, float pixel_step);

        void execute(Strip::Strip& strip, Iteration iteration) override;
    };
}

#endif // LEDZ_RAINBOW_H
