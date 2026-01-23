#include "Solid.h"
#include "../color.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace Show {
    Solid::Solid() : Solid(255, 255, 255) {
        // Default to white color
    }

    Solid::Solid(uint8_t red, uint8_t green, uint8_t blue)
        : target_color(color(red, green, blue)), blend_complete(false) {
        // Start with blend_complete=false to trigger smooth transition
    }

    Solid::Solid(Strip::Color color)
        : target_color(color), blend_complete(false) {
        // Start with blend_complete=false to trigger smooth transition
    }

    void Solid::setColor(uint8_t red, uint8_t green, uint8_t blue) {
        setColor(color(red, green, blue));
    }

    void Solid::setColor(Strip::Color color) {
        target_color = color;
        blend_complete = false;
        // Note: blend will be created in execute() when we have access to the strip
    }

    void Solid::execute(Strip::Strip &strip, Iteration iteration) {
        // If we need to start a blend and don't have one yet
        if (!blend_complete && blend == nullptr) {
#ifdef ARDUINO
            Serial.println("Solid: Starting new blend");
#endif
            blend = std::make_unique<Support::SmoothBlend>(strip, target_color);
        }

        // If blend is in progress
        if (blend != nullptr && !blend->isComplete()) {
            blend->step();
        } else if (blend != nullptr) {
            // Blend just completed
#ifdef ARDUINO
            Serial.println("Solid: Blend completed");
#endif
            blend.reset();
            blend_complete = true;
        }

        // Always set the color (either during blend or after)
        // SmoothBlend.step() already sets the colors during transition
        // So we only need to set when blend is complete
        if (blend_complete) {
            for (Strip::PixelIndex i = 0; i < strip.length(); i++) {
                strip.setPixelColor(i, target_color);
            }
        }
    }
} // namespace Show