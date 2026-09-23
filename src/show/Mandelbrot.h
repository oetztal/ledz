#ifndef LEDZ_MANDELBROT_H
#define LEDZ_MANDELBROT_H
#include "strip/Strip.h"
#include "Show.h"

namespace Show {
    class Mandelbrot : public Show {
        float c_re_min;
        float c_im_min;
        float c_im_max;
        unsigned int scale;
        unsigned int max_iterations;
        unsigned int color_scale;

        std::tuple<float, float> func(float zre, float zim, float cre, float cim) const;

    public:
        Mandelbrot(float cReMin, float cImMin, float cImMax, unsigned int scale, unsigned int max_iterations,
                   unsigned int colorScale);

        void log_result(unsigned long long j, float cre) const;

        void execute(Strip::Strip& strip, Iteration iteration) override;
    };
}

#endif // LEDZ_MANDELBROT_H
