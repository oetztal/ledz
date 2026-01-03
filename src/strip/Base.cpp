//
// Created by Andreas W. on 02.01.26.
//

#include "Base.h"

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
        strip = std::unique_ptr<Adafruit_NeoPixel>(new Adafruit_NeoPixel(length, pin, NEO_GRB + NEO_KHZ800));
        strip->begin();
        strip->setBrightness(20);
#endif
    }

    void Base::fill(Color c) const {
#ifdef ARDUINO
        strip->fill(c);
#endif
    }

    void Base::setPixelColor(PixelIndex pixel_index, Color color) {
#ifdef ARDUINO
        strip->setPixelColor(pixel_index, color);
#endif
    }

    Color Base::getPixelColor(PixelIndex pixel_index) const {
#ifdef ARDUINO
        return strip->getPixelColor(pixel_index);
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
}
