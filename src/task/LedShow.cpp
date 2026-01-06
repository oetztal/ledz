#include "LedShow.h"

#include "Timer.h"

namespace Task {
    void LedShow::startTask() {
#ifdef ARDUINO
        xTaskCreatePinnedToCore(
            taskWrapper, // Task Function
            "LED show", // Task Name
            10000, // Stack Size
            this, // Parameters
            2, // Priority
            &taskHandle, // Task Handle
            0 // Core Number (0)
        );
#endif
    }

#ifdef ARDUINO
    void LedShow::taskWrapper(void *pvParameters) {
        Serial.println("LedShow: taskWrapper()");
        auto *instance = static_cast<LedShow *>(pvParameters);
        instance->task();
    }

    void LedShow::task() {
        unsigned int iteration = 0;
        unsigned long total_execution_time = 0;
        unsigned long total_show_time = 0;

        unsigned long start_time = millis();
        unsigned long last_show_stats = millis();

        while (true) {
            // Process any pending show change commands from webserver
            controller.processCommands();

            auto timer = Support::Timer();

            controller.executeShow(iteration++);
            auto execution_time = timer.lap();

            controller.show();
            auto show_time = timer.lap();

            total_execution_time += execution_time;
            total_show_time += show_time;
            auto delay = 10 - std::min(10ul, timer.elapsed());
            // Log stats every 60 seconds to reduce Serial blocking
            if (timer.start_time - last_show_stats > 60000) {
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

    LedShow::LedShow(ShowController &controller) : controller(controller) {
    }
#endif
}
