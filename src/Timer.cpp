//
// Created by Andreas W. on 02.01.26.
//

#include "Timer.h"

#include <esp32-hal.h>

namespace Support {
    Timer::Timer() : start_time(millis()) {
        lap_start_time = start_time;
    }

    unsigned long Timer::lap() {
        auto lap_time = millis();
        auto result = lap_time - lap_start_time;
        lap_start_time = lap_time;
        return result;
    }

    unsigned long Timer::elapsed() {
        return millis() - start_time;
    }
} // Support
