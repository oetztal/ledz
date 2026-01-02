//
// Created by Andreas W. on 02.01.26.
//

#ifndef UNTITLED_LAYOUT_H
#define UNTITLED_LAYOUT_H
#include "Strip.h"

namespace Strip {
    class Layout : public Strip {
        Strip &strip;
        bool reverse;
        bool mirror;
        PixelIndex dead_leds;

        PixelIndex real_index(PixelIndex index) const;

    public:
        Layout(Strip &strip, bool reverse = false, bool mirror = false, PixelIndex dead_leds = 0);

        void fill(Color color) const override;

        void setPixelColor(PixelIndex pixel_index, Color color) override;

        Color getPixelColor(PixelIndex pixel_index) const override;

        PixelIndex length() const override;

        void show() override;
    };
}

#endif //UNTITLED_LAYOUT_H
