//
// Created by Andreas W. on 02.01.26.
//

#ifndef UNTITLED_TIMER_H
#define UNTITLED_TIMER_H

// Forward declare millis() for non-Arduino builds
#ifndef ARDUINO
unsigned long millis();
#endif

namespace Support {
    class Timer {
        unsigned long lap_start_time;

    public:
        Timer();

        unsigned long lap();

        unsigned long elapsed();

        const unsigned long start_time;
    };
} // Support

#endif //UNTITLED_TIMER_H