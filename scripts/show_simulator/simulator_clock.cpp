//
// Deterministic millis() for the host-side show simulator.
//
// The native Timer.cpp millis() implementation reads the wall clock, which
// makes SmoothBlend-driven shows (Solid / ColorRanges) non-reproducible
// across runs because the blend's progress depends on how fast the simulator
// loop happens to execute. The simulator wants to advance time at the same
// rate the device would: ~10 ms per execute() iteration, the period of the
// LED task on hardware. main.cpp nudges the counter once per call to
// execute() before the iteration's pixels are captured.
//
// Why a translation unit rather than a function in main.cpp: the simulator
// env builds main.o with -std=gnu++17 and ArduinoJson's include path
// doesn't pull Timer.h, so we own the millis() symbol entirely here.
//

#include <cstdint>

namespace {
    // LED task on the device runs at 100 Hz, so each execute() represents
    // 10 ms of wall time. The counter advances 10 ms per iteration, not
    // per second of real time.
    constexpr unsigned long kMillisPerIteration = 10UL;

    // Defaults to 0 so the first execute() reads the strip at t = 10 ms.
    unsigned long g_simulator_now_ms = 0UL;
}

// C++ linkage matches SmoothBlend.cpp's `::millis()` calls.
unsigned long millis() {
    return g_simulator_now_ms;
}

namespace SimulatorClock {

// Advance the simulator's notion of "now" by one iteration's worth of
// millis(). main.cpp calls this just before invoking execute().
void advance() {
    g_simulator_now_ms += kMillisPerIteration;
}

// Reset the clock back to zero so a fresh simulator process starts clean.
void reset() {
    g_simulator_now_ms = 0UL;
}

} // namespace SimulatorClock
