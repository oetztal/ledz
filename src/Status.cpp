//
// Created by Andreas W. on 02.01.26.
//

#include "Status.h"

namespace Status {
    
    Status::Status() {
#if defined(NEOPIXEL_POWER)
        // If this board has a power control pin, we must set it to output and high
        // in order to enable the NeoPixels. We put this in an #if defined so it can
        // be reused for other boards without compilation errors
        // pinMode(NEOPIXEL_POWER, OUTPUT);
        // digitalWrite(NEOPIXEL_POWER, HIGH);
#endif
        // pixel = Adafruit_NeoPixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
        // pixel.begin();
        // pixel.setBrightness(20);
        // setPixelColor(0x000000);
    }

    void Status::connecting() {
        Serial.println("Connecting...");
        setPixelColor(0xff0000);
    }

    void Status::connected() {
        Serial.println("Connected!");
        setPixelColor(0xff);
    }

    void Status::setBrightness(Strip::ColorComponent i) {
        // pixel.setBrightness(i);
    }

    void Status::setPixelColor(Strip::Color color) {
        // pixel.setPixelColor(0, color);
        // pixel.show();
    }
}