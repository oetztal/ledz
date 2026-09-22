#ifndef LEDZ_WS2812_H
#define LEDZ_WS2812_H
#include <cstdint>
#include <memory>
#include <vector>

#ifdef ARDUINO
#include "Adafruit_NeoPixel.h"
#endif
#include "Strip.h"

#ifdef ARDUINO
#include "../Config.h"
#endif

namespace Strip {
    class Base : public Strip {
#ifdef ARDUINO
        std::unique_ptr<Adafruit_NeoPixel> strip;
        std::vector<Color> colors;
        Config::GammaMode gammaMode;
        Brightness brightness;

#endif
    public:
        Base(Pin pin, unsigned short length);

        void fill(Color c) override;

        void setPixelColor(PixelIndex pixel_index, Color color) override;

        Color getPixelColor(PixelIndex pixel_index) const override;

        void show() override;

        PixelIndex length() const override;

        void setBrightness(Brightness brightness) override;

        [[nodiscard]] Brightness getBrightness() const override;

#ifdef ARDUINO
        /**
         * Set gamma correction mode
         * @param mode Gamma correction mode
         */
        void setGammaMode(Config::GammaMode mode);

    private:
        /**
         * Apply gamma correction based on current mode
         * @param color Input color
         * @return Gamma-corrected color
         */
        uint32_t applyGammaCorrection(uint32_t color);

        /**
         * Apply brightness scaling to a gamma-corrected color.
         * Each component is scaled by `brightness` using accurate
         * fixed-point math (FastLED-style scale8), avoiding the
         * integer rounding artifacts of the Adafruit library.
         * @param color Gamma-corrected color
         * @return Brightness-scaled color
         */
        uint32_t applyBrightness(uint32_t color);

        /**
         * Scale a single 8-bit channel by an 8-bit factor (0-255).
         * Uses (i * (1 + scale)) >> 8 so 255 * scale == scale exactly.
         */
        static uint8_t scaleComponent(uint8_t component, uint8_t scale);
#endif
    };
}

#endif // LEDZ_WS2812_H
