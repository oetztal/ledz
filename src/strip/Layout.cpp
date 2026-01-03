//
// Created by Andreas W. on 02.01.26.
//

#include "Layout.h"

namespace Strip {
    PixelIndex Layout::real_index(PixelIndex index) const {
        // if not 0 <= index < len(self):
        // raise IndexError("Index out of range")

        // Apply reverse first (within logical layout space)
        if (reverse) {
            index = length() - index - 1;
        }

        // Then apply dead LED offset to get physical strip index
        if (!mirror) {
            if (dead_leds > 0) {
                index += dead_leds;
            }
        } else {
            if (dead_leds < 0) {
                index += int(-dead_leds / 2);
            }
        }
        return index;
    }

    void Layout::fill(Color color) const {
        strip.fill(color);
    }

    void Layout::setPixelColor(PixelIndex pixel_index, Color color) {
        pixel_index = real_index(pixel_index);
        strip.setPixelColor(pixel_index, color);
        if (mirror) {
            strip.setPixelColor(strip.length() - pixel_index - 1, color);
        }
    }

    Color Layout::getPixelColor(PixelIndex pixel_index) const {
        return strip.getPixelColor(real_index(pixel_index));
    }

    PixelIndex Layout::length() const {
        return int((strip.length() - std::abs(dead_leds)) / (mirror ? 2 : 1));
    }

    void Layout::show() {
        strip.show();
    }

    Layout::Layout(Strip& strip, bool reverse, bool mirror, PixelIndex dead_leds) : strip(strip), reverse(reverse), mirror(mirror), dead_leds(dead_leds) {
    }
}
