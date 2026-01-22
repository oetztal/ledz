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
            1, // Priority
            &taskHandle, // Task Handle
            1 // Core Number
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
        unsigned long show_cycle_time = controller.getCycleTime();

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
            auto delay = show_cycle_time - std::min(show_cycle_time, timer.elapsed());

            // Update stats in controller every second (roughly)
            if (iteration % (1000 / std::max(1u, (unsigned int)show_cycle_time)) == 0) {
                ShowStats stats;
                stats.last_execution_time = execution_time;
                stats.last_show_time = show_time;
                stats.avg_execution_time = total_execution_time / iteration;
                stats.avg_show_time = total_show_time / iteration;
                stats.avg_cycle_time = (timer.start_time - start_time) / iteration;
                controller.updateStats(stats);
            }

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
