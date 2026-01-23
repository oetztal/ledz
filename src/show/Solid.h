// Solid - Display a single solid color across all LEDs
// Uses SmoothBlend for smooth color transitions
//

#ifndef LEDZ_SOLID_H
#define LEDZ_SOLID_H

#include "Show.h"
#include "../support/SmoothBlend.h"
#include <memory>

namespace Show {
    /**
     * Solid color show
     * Displays a single configurable color across all LEDs
     * Uses SmoothBlend for smooth transitions when color changes
     */
    class Solid : public Show {
    private:
        Strip::Color target_color;
        std::unique_ptr<Support::SmoothBlend> blend;
        bool blend_complete;

    public:
        /**
         * Create a solid color show with default white color
         */
        Solid();

        /**
         * Create a solid color show with a specific color
         * @param red Red component (0-255)
         * @param green Green component (0-255)
         * @param blue Blue component (0-255)
         */
        Solid(uint8_t red, uint8_t green, uint8_t blue);

        /**
         * Create a solid color show with a Color object
         * @param color The color to display
         */
        explicit Solid(Strip::Color color);

        /**
         * Set a new color (will trigger smooth transition)
         * @param red Red component (0-255)
         * @param green Green component (0-255)
         * @param blue Blue component (0-255)
         */
        void setColor(uint8_t red, uint8_t green, uint8_t blue);

        /**
         * Set a new color (will trigger smooth transition)
         * @param color The new color
         */
        void setColor(Strip::Color color);

        /**
         * Execute one iteration of the show
         */
        void execute(Strip::Strip &strip, Iteration iteration) override;
    };
} // namespace Show

#endif //LEDZ_SOLID_H