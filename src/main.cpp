//
// Created by Andreas W. on 02.01.26.
//

#include <memory>

#include "show/ColorRun.h"
#include "strip/WS2812.h"
#include "show/Rainbow.h"


// How many internal neopixels do we have? some boards have more than one!
#define NUMPIXELS        300

// Adafruit_NeoPixel pixels(NUMPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
// Adafruit_NeoPixel pixels(NUMPIXELS, MOSI, NEO_GRB + NEO_KHZ800);

std::unique_ptr<Strip::Strip> strip{new Strip::WS2812(MOSI, NUMPIXELS)};

std::unique_ptr<Show::Show> show;
void setup() {
    // show.reset(new Show::Rainbow());
    show.reset(new Show::ColorRun());
}

unsigned int iteration = 0;

void loop() {
    show->execute(*strip, iteration);

    delay(1);
    iteration += 1;
}
