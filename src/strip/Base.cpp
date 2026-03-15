#include "Base.h"
#include "../support/Gamma.h"

namespace Strip {
    Base::Base(Pin pin, unsigned short length) {
#ifdef ARDUINO
#if defined(NEOPIXEL_POWER)
        // If this board has a power control pin, we must set it to output and high
        // in order to enable the NeoPixels. We put this in an #if defined so it can
        // be reused for other boards without compilation errors
        if (pin == PIN_NEOPIXEL) {
            pinMode(NEOPIXEL_POWER, OUTPUT);
            digitalWrite(NEOPIXEL_POWER, HIGH);
        }
#endif
        strip = std::make_unique<Adafruit_NeoPixel>(length, pin, NEO_GRB + NEO_KHZ800);
        colors = std::unique_ptr<Color[]>(new Color[length]);
        strip->begin();
        // Brightness will be set by ShowController
        // Default to improved gamma correction
        gammaMode = Config::GAMMA_DEFAULT;
#endif
    }

    void Base::fill(Color c) {
#ifdef ARDUINO
        for (int i=0; i<strip->numPixels(); i++) {
            colors[i]=c;
        }

        strip->fill(applyGammaCorrection(c));
#endif
    }

    void Base::setPixelColor(PixelIndex pixel_index, Color color) {
#ifdef ARDUINO
        colors[pixel_index]=color;
        strip->setPixelColor(pixel_index, applyGammaCorrection(color));
#endif
    }

    Color Base::getPixelColor(PixelIndex pixel_index) const {
#ifdef ARDUINO
        return colors[pixel_index];
#else
        return 0;
#endif
    }

    void Base::show() {
#ifdef ARDUINO
        strip->show();
#endif
    }

    PixelIndex Base::length() const {
#ifdef ARDUINO
        return strip->numPixels();
#else
        return 0;
#endif
    }

    void Base::setBrightness(uint8_t brightness) {
#ifdef ARDUINO
        auto currentBrightness = strip->getBrightness();
        if (currentBrightness != brightness) {
            for (int i=0; i<strip->numPixels(); i++) {
                strip->setPixelColor(i, applyGammaCorrection(colors[i]));
            }
            strip->setBrightness(brightness);
        }
#endif
    }

#ifdef ARDUINO
    void Base::setGammaMode(Config::GammaMode mode) {
        gammaMode = mode;
        Serial.printf("Gamma mode set to: %d\n", mode);
    }

    uint32_t Base::applyGammaCorrection(uint32_t color) {
        switch (gammaMode) {
            case Config::GAMMA_NONE:
                // No gamma correction
                return color;
            case Config::GAMMA_NEOPIXEL:
                // Use Adafruit NeoPixel gamma correction
                return Adafruit_NeoPixel::gamma32(color);
            case Config::GAMMA_DEFAULT:
            default:
                // Use our improved gamma correction
                return Support::Gamma::correct32(color);
        }
    }
#endif
}
