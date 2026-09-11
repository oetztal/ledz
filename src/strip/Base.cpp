#include "Base.h"
#include "../Log.h"
#include "../support/Gamma.h"

static const char* TAG = "strip";

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
        // Pin Adafruit's internal brightness at 255 so it does not
        // mutate our pixel data with its integer-based scaler.
        // Application-level brightness is applied in setPixelColor/fill.
        strip->setBrightness(255);
        brightness = 255;
        // Default to improved gamma correction
        gammaMode = Config::GAMMA_DEFAULT;
#endif
    }

    void Base::fill(Color c) {
#ifdef ARDUINO
        uint32_t scaled = applyBrightness(applyGammaCorrection(c));
        for (int i=0; i<strip->numPixels(); i++) {
            colors[i]=c;
        }

        strip->fill(scaled);
#endif
    }

    void Base::setPixelColor(PixelIndex pixel_index, Color color) {
#ifdef ARDUINO
        colors[pixel_index]=color;
        strip->setPixelColor(pixel_index, applyBrightness(applyGammaCorrection(color)));
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

    void Base::setBrightness(Brightness newBrightness) {
#ifdef ARDUINO
        if (brightness != newBrightness) {
            brightness = newBrightness;
            // Re-emit every cached pixel with the new brightness factor.
            // Adafruit's setBrightness is never called: it stays pinned at 255
            // so the hardware buffer is written verbatim.
            for (int i=0; i<strip->numPixels(); i++) {
                strip->setPixelColor(i, applyBrightness(applyGammaCorrection(colors[i])));
            }
        }
#endif
    }

    Brightness Base::getBrightness() const {
#ifdef ARDUINO
        return brightness;
#else
        return 255;
#endif
    }

#ifdef ARDUINO
    void Base::setGammaMode(Config::GammaMode mode) {
        gammaMode = mode;
        ESP_LOGI(TAG, "Gamma mode set to: %d", mode);
    }

    uint32_t Base::applyGammaCorrection(uint32_t color) {
        switch (gammaMode) {
            case Config::GAMMA_NONE:
                return color;
            case Config::GAMMA_NEOPIXEL:
                return Adafruit_NeoPixel::gamma32(color);
            case Config::GAMMA_DEFAULT:
            default:
                return Support::Gamma::correct32(color);
        }
    }

    uint8_t Base::scaleComponent(uint8_t component, uint8_t scale) {
        // FastLED-style scale8: (i * (1 + scale)) >> 8
        // This guarantees scale8(255, s) == s and is free of the
        // integer-truncation banding that (i * scale) >> 8 produces
        // (e.g. (255 * 255) >> 8 == 254 with the naive form).
        return static_cast<uint8_t>(
            (static_cast<uint16_t>(component) * (static_cast<uint16_t>(scale) + 1)) >> 8
        );
    }

    uint32_t Base::applyBrightness(uint32_t color) {
        if (brightness == 255) {
            return color;
        }
        uint8_t r = scaleComponent(static_cast<uint8_t>((color >> 16) & 0xFF), brightness);
        uint8_t g = scaleComponent(static_cast<uint8_t>((color >> 8) & 0xFF), brightness);
        uint8_t b = scaleComponent(static_cast<uint8_t>(color & 0xFF), brightness);
        return (static_cast<uint32_t>(r) << 16) |
               (static_cast<uint32_t>(g) << 8)  |
                static_cast<uint32_t>(b);
    }
#endif
}
