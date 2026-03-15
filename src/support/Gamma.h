#ifndef LEDZ_GAMMA_H
#define LEDZ_GAMMA_H

#include <cstdint>

namespace Support {
    /**
     * Improved gamma correction for LED color mixing.
     * Uses a more moderate gamma curve (γ=2.2) that provides better
     * color mixing while maintaining good visual perception.
     * Also includes optional linear interpolation for smoother transitions.
     */
    class Gamma {
    public:
        /**
         * Apply gamma correction to a single 8-bit color channel
         * @param x Input value (0-255)
         * @return Gamma-corrected value (0-255)
         */
        static uint8_t correct8(uint8_t x);

        /**
         * Apply gamma correction to a 32-bit packed RGB color
         * @param color Input color in 0xRRGGBB format
         * @return Gamma-corrected color
         */
        static uint32_t correct32(uint32_t color);

        /**
         * Apply inverse gamma correction (for color picking, etc.)
         * @param x Gamma-corrected value (0-255)
         * @return Linear value (0-255)
         */
        static uint8_t uncorrect8(uint8_t x);

        /**
         * Apply inverse gamma correction to a 32-bit packed RGB color
         * @param color Gamma-corrected color in 0xRRGGBB format
         * @return Linear color
         */
        static uint32_t uncorrect32(uint32_t color);

    private:
        // Pre-computed gamma correction table for performance
        static const uint8_t gammaTable[256];
        static const uint8_t invGammaTable[256];
    };
} // namespace Support

#endif //LEDZ_GAMMA_H
