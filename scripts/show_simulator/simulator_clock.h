#ifndef SHOW_SIMULATOR_SIMULATOR_CLOCK_H
#define SHOW_SIMULATOR_SIMULATOR_CLOCK_H

namespace SimulatorClock {

// Advance the deterministic millis() clock by one LED-tick (10 ms). Called
// by main.cpp just before each execute() so SmoothBlend-driven shows see
// the same elapsed time across runs regardless of host CPU speed.
void advance();

// Reset the clock to zero so each simulator process starts from t=0.
void reset();

} // namespace SimulatorClock

#endif // SHOW_SIMULATOR_SIMULATOR_CLOCK_H
