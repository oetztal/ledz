//
// Created by Andreas W. on 02.01.26.
//

#include <memory>

#include "Network.h"
#include "Status.h"
#include "Timer.h"
#include "Config.h"
#include "WebServerManager.h"
#include "ShowFactory.h"
#include "ShowController.h"
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
#endif

#ifdef ARDUINO
[[noreturn]] void ledShowTask(void *pvParameters) {
    auto* controller = static_cast<ShowController*>(pvParameters);

    unsigned int iteration = 0;
    unsigned long total_execution_time = 0;
    unsigned long total_show_time = 0;

    unsigned long start_time = millis();
    unsigned long last_show_stats = millis();

    while (true) {
        // Process any pending show change commands from webserver
        controller->processCommands();

        // Get current show
        Show::Show* show = controller->getCurrentShow();
        if (show == nullptr) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            continue;
        }

        // Execute show
        auto timer = Support::Timer();

        show->execute(*layout, iteration++);
        auto execution_time = timer.lap();

        layout->show();
        auto show_time = timer.lap();

        total_execution_time += execution_time;
        total_show_time += show_time;
        auto delay = 10 - std::min(10ul, timer.elapsed());
        if (timer.start_time - last_show_stats > 10000) {
            Serial.printf(
                "Durations: execution %lu ms (avg: %lu ms), show %lu ms (avg: %lu ms), avg. cycle %lu ms, delay %lu ms\n",
                execution_time, total_execution_time / iteration,
                show_time, total_show_time / iteration,
                (timer.start_time - start_time) / iteration, delay);
            last_show_stats = timer.start_time;
        }
        vTaskDelay(delay / portTICK_PERIOD_MS);
    }
}

TaskHandle_t showTaskHandle = nullptr;
TaskHandle_t networkTaskHandle = nullptr;

Status::Status status;
Config::ConfigManager config;
ShowFactory showFactory;
ShowController showController(showFactory, config);
Network network(config, status);
WebServerManager webServer(config, network);

void setup() {
    // Serial.begin(115200);

    // Initialize configuration manager
    config.begin();

    // Initialize show controller
    showController.begin();

    // Set webserver on network (must be done before network task starts)
    network.setWebServer(&webServer);

    // Create LED show task on Core 0
    xTaskCreatePinnedToCore(
        ledShowTask, // Task Function
        "LED show", // Task Name
        10000, // Stack Size
        &showController, // Parameters (pass controller)
        1, // Priority
        &showTaskHandle, // Task Handle
        0 // Core Number (0)
    );

    // Create Network task on Core 1
    xTaskCreatePinnedToCore(
        Network::taskWrapper, // Task Function
        "Network", // Task Name
        10000, // Stack Size
        &network, // Parameters
        2, // Priority
        &networkTaskHandle, // Task Handle
        1 // Core Number (1)
    );
}

void loop() {
#ifdef ARDUINO
    // Auto-cycling is now handled by ShowController and webserver
    // This loop is just for Arduino framework compatibility
    vTaskDelay(60000 / portTICK_PERIOD_MS);
#endif
}
#endif
