//
// Created by Andreas W. on 02.01.26.
//

#ifndef UNTITLED_MANDELBROT_H
#define UNTITLED_MANDELBROT_H
#include "strip/Strip.h"
#include "Show.h"

namespace Show {
    class Mandelbrot : public Show {
        float c_re_min, c_im_min, c_im_max;
        unsigned int scale;
        unsigned int max_iterations;
        unsigned int color_scale;

        std::pair<float, float> func(float zre, float zim, float cre, float cim);

    public:
        Mandelbrot(float cReMin, float cImMin, float cImMax, unsigned int scale = 5, unsigned int max_iterations = 50,
                   unsigned int colorScale = 10);

        void log_result(unsigned long long j, float cre);

        void execute(Strip::Strip &strip, Iteration iteration) override;
    };
}

#endif //UNTITLED_MANDELBROT_H
