//
// Created by Andreas W. on 02.01.26.
//

#include <memory>

#include "Network.h"
#include "Status.h"
#include "Timer.h"
#ifdef ARDUINO
#include <codecvt>
#endif
#include "show/Chaos.h"
#include "show/ColorRun.h"
#include "show/Jump.h"
#include "show/Mandelbrot.h"
#include "strip/Base.h"
#include "show/Rainbow.h"
#include "strip/Layout.h"


// How many internal neopixels do we have? some boards have more than one!
#define NUMPIXELS        300

#ifdef ARDUINO
std::unique_ptr<Strip::Strip> base{new Strip::Base(MOSI, NUMPIXELS)};
std::unique_ptr<Strip::Strip> layout{new Strip::Layout(*base)};
bool reverse = false;


std::unique_ptr<Show::Show> show;
#endif

#ifdef ARDUINO
[[noreturn]] void ledShowTask(void *pvParameters) {
    unsigned int iteration = 0;
    unsigned long total_execution_time = 0;
    unsigned long total_show_time = 0;

    unsigned long start_time = millis();
    unsigned long last_show_stats = millis();

    while (true) {
        // layout.reset(new Strip::Layout(*base, true, true));
        auto timer = Support::Timer();

        show->execute(*layout, iteration++);
        auto execution_time = timer.lap();

        layout->show();
        auto show_time = timer.lap();

        total_execution_time += execution_time;
        total_show_time += show_time;
        auto delay = 10 - std::min(10ul, timer.elapsed());
        if (timer.start_time - last_show_stats > 10000) {
#ifdef ARDUINO
            Serial.printf(
                "Durations: execution %lu ms (avg: %lu ms), show %lu ms (avg: %lu ms), avg. cycle %lu ms, delay %lu ms\n",
                execution_time, total_execution_time / iteration,
                show_time, total_show_time / iteration,
                (timer.start_time - start_time) / iteration, delay);
#endif
            last_show_stats = timer.start_time;
        }
        vTaskDelay(delay / portTICK_PERIOD_MS);
    }
}

TaskHandle_t showTaskHandle = nullptr;
TaskHandle_t networkTaskHandle = nullptr;

Status::Status status;
Network network("Xenia", "Internet!bei!uns", status);

void setup() {
    // Serial.begin(115200);

    show.reset(new Show::Rainbow());
    //show.reset(new Show::Mandelbrot(-1.05, -0.3616, -0.3156));
    //show.reset(new Show::Chaos());

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

unsigned int effect = 0;

void loop() {
#ifdef ARDUINO
    Serial.println("main loop");
#endif
    vTaskDelay(60000 / portTICK_PERIOD_MS);

    switch (effect++ % 4) {
        case 0:
            show.reset(new Show::Mandelbrot(-1.05, -0.3616, -0.3156));
            break;
        case 1:
            show.reset(new Show::Chaos());
            break;
        case 2:
            show.reset(new Show::ColorRun());
            break;
        case 3:
            show.reset(new Show::Jump());
            break;
        case 4:
            show.reset(new Show::Rainbow());
            break;
    }
}
#endif
