//
// Created by Andreas W. on 02.01.26.
//

#ifndef UNTITLED_WS2812_H
#define UNTITLED_WS2812_H
#include <cstdint>
#include <memory>

#ifdef ARDUINO
#include "Adafruit_NeoPixel.h"
#endif
#include "Strip.h"

namespace Strip {

    class Base : public Strip {
    public:
        Base(Pin pin, unsigned short length);

        void fill(Color c) const override;

        void setPixelColor(PixelIndex pixel_index, Color color) override;

        Color getPixelColor(PixelIndex pixel_index) const override;

        void show() override;

        PixelIndex length() const override;

        void setBrightness(uint8_t brightness) override;

    private:
#ifdef ARDUINO
        std::unique_ptr<Adafruit_NeoPixel> strip;
#endif
    };
}

#endif //UNTITLED_WS2812_H
