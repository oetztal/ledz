//
// Created by Andreas W. on 02.01.26.
//

#ifndef UNTITLED_STATUS_H
#define UNTITLED_STATUS_H
#include "Adafruit_NeoPixel.h"
#include "strip/Strip.h"

namespace Status {
    class Status {
        // Adafruit_NeoPixel pixel;

        void setPixelColor(Strip::Color color);

    public:
        Status();

        void connecting();

        void connected();

        void setBrightness(Strip::ColorComponent i);
    };
}


#endif //UNTITLED_STATUS_H
