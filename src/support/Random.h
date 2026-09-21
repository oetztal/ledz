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

    namespace detail {
        // Function-local static in randomSeed() holds the override. Set by
        // setRandomSeedOverride() (which reaches it through the same static).
        // Sentinel -1 means "no override"; kept as a signed value so any caller-
        // supplied unsigned value can never accidentally hit it. C++11 guarantees
        // thread-safe first-read initialisation, so no lock is needed.
        inline Random::result_type &seedOverride() {
            static Random::result_type override = -1;
            return override;
        }
    }

    /**
     * Force the next randomSeed() call to return a fixed value.
     *
     * Used by the host-side show simulator ([env:native_show_sim]) to make
     * random shows (Fire, ColorRun, Starlight) reproducible across runs.
     * The override is held until cleared; pass any value < 0 to clear it.
     *
     * On the device the override is never set, so randomSeed() always
     * returns hardware entropy — no firmware behaviour change.
     */
    inline void setRandomSeedOverride(Random::result_type seed) {
        detail::seedOverride() = seed;
    }

    /**
     * Obtain a seed for Random.
     * @return the active override if one is set, otherwise hardware entropy
     *         on the ESP32, wall clock elsewhere
     */
    inline Random::result_type randomSeed() {
        const auto override = detail::seedOverride();
        if (override >= 0) {
            return override;
        }
#ifdef ARDUINO
        return static_cast<Random::result_type>(esp_random());
#else
        return static_cast<Random::result_type>(
            std::chrono::steady_clock::now().time_since_epoch().count());
#endif
    }
} // Support

#endif //LEDZ_RANDOM_H
