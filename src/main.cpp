//
// Created by Andreas W. on 02.01.26.
//

#include <memory>

#include "Network.h"
#include "Status.h"
#include "show/ColorRun.h"
#include "show/Mandelbrot.h"
#include "strip/Base.h"
#include "show/Rainbow.h"
#include "strip/Layout.h"


// How many internal neopixels do we have? some boards have more than one!
#define NUMPIXELS        300

std::unique_ptr<Strip::Strip> base{new Strip::Base(MOSI, NUMPIXELS)};
std::unique_ptr<Strip::Strip> layout{new Strip::Layout(*base)};
bool reverse = false;

unsigned int iteration = 0;

std::unique_ptr<Show::Show> show;

[[noreturn]] void ledShowTask(void *pvParameters) {
    while (true) {
        // Serial.println("show loop");
        // layout.reset(new Strip::Layout(*base, true, true));
        show->execute(*layout, iteration++);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

TaskHandle_t showTaskHandle = nullptr;
TaskHandle_t networkTaskHandle = nullptr;

Status::Status status;
Network network("Xenia", "Internet!bei!uns", status);

void setup() {
    // Serial.begin(115200);

    // show.reset(new Show::Rainbow());
    show.reset(new Show::Mandelbrot::Mandelbrot(-1.05, -0.3616, -0.3156));

    // Create Task1 on Core 0
    xTaskCreatePinnedToCore(
        ledShowTask, // Task Function
        "LED show", // Task Name
        10000, // Stack Size
        nullptr, // Parameters
        1, // Priority
        &showTaskHandle, // Task Handle
        0 // Core Number (0)
    );

    xTaskCreatePinnedToCore(
        Network::taskWrapper, // Task Function
        "Network", // Task Name
        10000, // Stack Size
        &network, // Parameters
        2, // Priority
        &networkTaskHandle, // Task Handle
        1 // Core Number (0)
    );
}

void loop() {
    Serial.println("main loop");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}
