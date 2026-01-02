//
// Created by Andreas W. on 02.01.26.
//

#include "WS2812.h"

namespace Strip {
    WS2812::WS2812(Pin pin, unsigned short length) {
#if defined(NEOPIXEL_POWER)
        // If this board has a power control pin, we must set it to output and high
        // in order to enable the NeoPixels. We put this in an #if defined so it can
        // be reused for other boards without compilation errors
        if (pin == PIN_NEOPIXEL) {
            pinMode(NEOPIXEL_POWER, OUTPUT);
            digitalWrite(NEOPIXEL_POWER, HIGH);
        }
#endif
        strip = std::unique_ptr<Adafruit_NeoPixel>(new Adafruit_NeoPixel(length, pin, NEO_GRB + NEO_KHZ800));
        strip->begin();
        strip->setBrightness(20);
    }

    void WS2812::fill(Color c) const {
        strip->fill(c);
    }

    void WS2812::setPixelColor(PixelIndex pixel_index, Color color) {
        strip->setPixelColor(pixel_index, color);
    }

    Color WS2812::getPixelColor(PixelIndex pixel_index) const {
        return strip->getPixelColor(pixel_index);
    }

    void WS2812::show() {
        strip->show();
    }

    PixelIndex WS2812::length() const {
        return strip->numPixels();
    }
}
