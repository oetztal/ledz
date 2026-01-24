#ifndef LEDZ_COLORRANGES_H
#define LEDZ_COLORRANGES_H

#include "Show.h"
#include "../support/SmoothBlend.h"
#include <memory>
#include <vector>

namespace Show {
    /**
     * Blend type for transitions between adjacent colors
     */
    enum class BlendType {
        NONE,    // Sharp transition (current behavior)
        LINEAR   // Linear interpolation between colors
    };

    /**
     * ColorRanges - Displays color sections across the LED strip
     * Perfect for single solid colors, flags, multi-color patterns, and gradients
     * Supports 1 or more colors with configurable blend types between them
     */
    class ColorRanges : public Show {
    private:
        std::vector<Strip::Color> colors; // List of colors to display
        std::vector<float> ranges; // Boundary percentages (0-100)
        std::vector<BlendType> blends; // Blend types between adjacent colors
        std::unique_ptr<Support::SmoothBlend> blend;
        bool initialized;

    public:
        /**
         * Constructor with colors, optional ranges, and optional blends
         * @param colors Vector of colors to display
         * @param ranges Vector of boundary percentages (0-100). If empty, colors distribute equally.
         *               Example: [30, 70] with 3 colors creates: color1 (0-30%), color2 (30-70%), color3 (70-100%)
         * @param blends Vector of blend types for transitions between colors. If empty, all transitions use NONE.
         *               Example: [LINEAR, NONE] with 3 colors: linear blend color1->color2, sharp transition color2->color3
         */
        ColorRanges(const std::vector<Strip::Color> &colors,
                    const std::vector<float> &ranges = {},
                    const std::vector<BlendType> &blends = {});

        /**
         * Execute the show - creates color ranges and smoothly blends to them
         * @param strip LED strip to control
         * @param iteration Current iteration number
         */
        void execute(Strip::Strip &strip, Iteration iteration) override;

        const char *name() { return "ColorRanges"; }
    };
} // namespace Show

#endif //LEDZ_COLORRANGES_H