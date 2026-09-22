#include "Stroboscope.h"
#include "../support/Color.h"

namespace Show {
    Stroboscope::Stroboscope(uint8_t r, uint8_t g, uint8_t b,
                             unsigned int on_cycles, unsigned int off_cycles)
        : r(r), g(g), b(b), on_cycles(on_cycles), off_cycles(off_cycles) {
    }

    void Stroboscope::execute(Strip::Strip &strip, Iteration iteration) {
        // Calculate total cycle length
        unsigned int total_cycles = on_cycles + off_cycles;

        // Check if we're in the "on" phase
        if (unsigned int cycle_position = current_cycle % total_cycles; cycle_position < on_cycles) {
            // Flash the color
            Strip::Color flash_color = Support::Color::from_rgb(r, g, b);
            strip.fill(flash_color);
        } else {
            // Stay black
            Strip::Color black = Support::Color::from_rgb(0, 0, 0);
            strip.fill(black);
        }

        // Increment cycle counter
        current_cycle++;
    }
} // namespace Show