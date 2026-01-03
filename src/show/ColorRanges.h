//
// Created by Claude Code on 03.01.26.
//

#ifndef UNTITLED_COLORRANGES_H
#define UNTITLED_COLORRANGES_H

#include "Show.h"
#include "../support/SmoothBlend.h"
#include <memory>
#include <vector>

namespace Show {

    /**
     * ColorRanges - Displays solid color sections across the LED strip
     * Perfect for flags, multi-color patterns with sharp transitions
     */
    class ColorRanges : public Show {
    private:
        std::vector<Strip::Color> colors;     // List of colors to display
        std::vector<float> ranges;            // Boundary percentages (0-100)
        std::unique_ptr<Support::SmoothBlend> blend;
        bool initialized;

    public:
        /**
         * Constructor with colors and optional ranges
         * @param colors Vector of colors to display
         * @param ranges Vector of boundary percentages (0-100). If empty, colors distribute equally.
         *               Example: [30, 70] with 3 colors creates: color1 (0-30%), color2 (30-70%), color3 (70-100%)
         */
        ColorRanges(const std::vector<Strip::Color>& colors, const std::vector<float>& ranges = {});

        /**
         * Execute the show - creates color ranges and smoothly blends to them
         * @param strip LED strip to control
         * @param iteration Current iteration number
         */
        void execute(Strip::Strip &strip, Iteration iteration) override;

        const char *name() { return "ColorRanges"; }
    };

} // namespace Show

#endif //UNTITLED_COLORRANGES_H
