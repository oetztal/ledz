#ifndef LEDZ_WS2812_H
#define LEDZ_WS2812_H
#include <cstdint>
#include <memory>

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
        std::unique_ptr<Color[]> colors;
        Config::GammaMode gammaMode;
#endif
    public:
        Base(Pin pin, unsigned short length);

        void fill(Color c) override;

        void setPixelColor(PixelIndex pixel_index, Color color) override;

        Color getPixelColor(PixelIndex pixel_index) const override;

        void show() override;

        PixelIndex length() const override;

        void setBrightness(uint8_t brightness) override;

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
#endif

    };
}

#endif //LEDZ_WS2812_H