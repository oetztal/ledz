//
// Created by Andreas W. on 02.01.26.
//

#ifndef UNTITLED_WS2812_H
#define UNTITLED_WS2812_H
#include <cstdint>
#include <memory>

#include "Adafruit_NeoPixel.h"
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

    private:
        std::unique_ptr<Adafruit_NeoPixel> strip;
    };
}

#endif //UNTITLED_WS2812_H
