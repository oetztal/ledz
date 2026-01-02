//
// Created by Andreas W. on 02.01.26.
//

#include <sstream>

#include "Mandelbrot.h"

#include <USBCDC.h>

#include "color.h"

namespace Show {
    std::pair<float, float> Mandelbrot::func(float zre, float zim, float cre, float cim) {
        // z_n+1 = z_n^2 + c
        return std::pair<float, float>(zre * zre - zim * zim + cre, 2 * zre * zim + cim);
    }

    Mandelbrot::Mandelbrot(float cReMin, float cImMin, float cImMax, unsigned int scale,
                                 unsigned int max_iterations, unsigned int colorScale) : c_re_min(cReMin),
        c_im_min(cImMin), c_im_max(cImMax), scale(scale), max_iterations(max_iterations), color_scale(colorScale) {
    }

    void Mandelbrot::log_result(unsigned long long j, float cre) {
        std::stringstream ss;
        ss << j << "(" << cre << ") [" << c_im_min << ", " << c_im_max << "], " << max_iterations;
        Serial.println(ss.str().c_str());
    }

    void Mandelbrot::execute(Strip::Strip &strip, Iteration iteration) {
        float cDelta = abs(c_im_max - c_im_min) / strip.length();

        auto j = iteration % (strip.length() * scale);
        float cre = c_re_min + (cDelta / scale) * j;

        unsigned int line_max_iterations = 0;

        for (unsigned int i = 0; i < strip.length(); i++) {
            float cim = c_im_min + cDelta * i;

            float zre = 0.0, zim = 0.0;

            unsigned int iterations = max_iterations;
            for (int k = 0; k < max_iterations; k++) {
                auto pair = func(zre, zim, cre, cim);
                zre = pair.first;
                zim = pair.second;

                if (zre * zre + zim * zim > 10) {
                    iterations = k;
                    break;
                }
            }

            Strip::Color color;
            if (iterations < max_iterations) {
                color = wheel((iterations * color_scale) % 255);
            } else {
                color = 0x000000;
            }

            strip.setPixelColor(i, color);

            line_max_iterations = std::max(line_max_iterations, iterations);
        }

        // log_result(j, cre);
    }
}
