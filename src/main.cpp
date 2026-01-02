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


unsigned int iteration = 0;

std::unique_ptr<Show::Show> show;

[[noreturn]] void ledShowTask(void *pvParameters) {
    while (true) {
        Serial.println("show loop");
        show->execute(*strip, iteration++);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

TaskHandle_t Task1Handle = nullptr;


void setup() {
    // Serial.begin(115200);

    // show.reset(new Show::Rainbow());
    show.reset(new Show::ColorRun());

    // Create Task1 on Core 0
    xTaskCreatePinnedToCore(
        ledShowTask, // Task Function
        "LED show", // Task Name
        10000, // Stack Size
        nullptr, // Parameters
        1, // Priority
        &Task1Handle, // Task Handle
        0 // Core Number (0)
    );
}

void loop() {
    Serial.println("main loop");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}
