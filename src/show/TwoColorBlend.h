#ifndef LEDZ_TWOCOLORBLEND_H
#define LEDZ_TWOCOLORBLEND_H

#include "Show.h"
#include "../support/SmoothBlend.h"
#include <memory>

namespace Show {
    /**
     * TwoColorBlend - Creates a linear gradient from one color at the start
     * of the strip to another color at the end, with smooth transitions
     */
    class TwoColorBlend : public Show {
    private:
        Strip::Color color1; // Starting color (beginning of strip)
        Strip::Color color2; // Ending color (end of strip)
        std::unique_ptr<Support::SmoothBlend> blend;
        bool gradient_initialized;

    public:
        /**
         * Constructor with two RGB colors
         * @param r1 Red component of color1 (0-255)
         * @param g1 Green component of color1 (0-255)
         * @param b1 Blue component of color1 (0-255)
         * @param r2 Red component of color2 (0-255)
         * @param g2 Green component of color2 (0-255)
         * @param b2 Blue component of color2 (0-255)
         */
        TwoColorBlend(uint8_t r1, uint8_t g1, uint8_t b1,
                      uint8_t r2, uint8_t g2, uint8_t b2);

        /**
         * Execute the show - creates gradient and smoothly blends to it
         * @param strip LED strip to control
         * @param iteration Current iteration number
         */
        void execute(Strip::Strip &strip, Iteration iteration) override;

        const char *name() { return "TwoColorBlend"; }
    };
} // namespace Show

#endif //LEDZ_TWOCOLORBLEND_H