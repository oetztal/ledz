#ifndef LEDZ_RANDOM_H
#define LEDZ_RANDOM_H

#include <cstdint>
#include <random>

#ifdef ARDUINO
#include <esp_system.h>
#else
#include <chrono>
#endif

namespace Support {
    /**
     * Lightweight PRNG for shows.
     *
     * std::mt19937 is deliberately avoided: its state is ~2.5 KB, and seeding it
     * from std::random_device costs another ~2.5 KB of stack (this toolchain has
     * no entropy device, so random_device falls back to an embedded mt19937).
     * That combination overflows the 8 KB Arduino loop task stack when a show is
     * constructed from setup(). minstd_rand holds 4 bytes and is ample here.
     */
    using Random = std::minstd_rand;

    /**
     * Obtain a seed for Random.
     * @return hardware entropy on the ESP32, wall clock elsewhere
     */
    inline Random::result_type randomSeed() {
#ifdef ARDUINO
        return static_cast<Random::result_type>(esp_random());
#else
        return static_cast<Random::result_type>(
            std::chrono::steady_clock::now().time_since_epoch().count());
#endif
    }
} // Support

#endif //LEDZ_RANDOM_H
