//
// Created by Claude Code on 03.01.26.
//

#include "ShowFactory.h"
#include "show/Rainbow.h"
#include "show/ColorRanges.h"
#include "show/ColorRun.h"
#include "show/Mandelbrot.h"
#include "show/Chaos.h"
#include "show/Jump.h"
#include "show/Solid.h"
#include "show/Starlight.h"
#include "show/TwoColorBlend.h"
#include "show/Wave.h"
#include "show/MorseCode.h"
#include "show/TheaterChase.h"
#include "show/Stroboscope.h"
#include "color.h"

#ifdef ARDUINO
#include <ArduinoJson.h>
#endif

ShowFactory::ShowFactory() : showConstructors(strLess) {
    // Register all available shows (in display order)
    // Each lambda receives a StaticJsonDocument and uses defaults via | operator

    registerShow("Solid", "Solid color (default: white)", [](const StaticJsonDocument<512> &doc) {
        uint8_t r = doc["r"] | 255; // Default to white
        uint8_t g = doc["g"] | 255;
        uint8_t b = doc["b"] | 255;
#ifdef ARDUINO
        Serial.printf("ShowFactory: Creating Solid with color RGB(%d,%d,%d)\n", r, g, b);
#endif
        return std::make_unique<Show::Solid>(r, g, b);
    });

    registerShow("ColorRanges", "Single color or color sections (flags, patterns)", [](const StaticJsonDocument<512> &doc) {
        std::vector<Strip::Color> colors;
        std::vector<float> ranges;

#ifdef ARDUINO
        Serial.println("ShowFactory: Parsing ColorRanges parameters");
        Serial.print("JSON: ");
        serializeJson(doc, Serial);
        Serial.println();
#endif

        // Check if using single color format (like Solid show)
        if (doc.containsKey("r") || doc.containsKey("g") || doc.containsKey("b")) {
            uint8_t r = doc["r"] | 255; // Default to white if not specified
            uint8_t g = doc["g"] | 255;
            uint8_t b = doc["b"] | 255;
            colors.push_back(color(r, g, b));
#ifdef ARDUINO
            Serial.printf("Single color format: RGB(%d,%d,%d)\n", r, g, b);
#endif
        }
        // Parse colors array (multi-color format)
        else if (doc.containsKey("colors") && doc["colors"].is<JsonArray>()) {
            JsonArrayConst colorsArray = doc["colors"];
#ifdef ARDUINO
            Serial.printf("Found %d colors in array\n", colorsArray.size());
#endif
            for (JsonVariantConst colorVariant: colorsArray) {
                if (colorVariant.is<JsonArray>()) {
                    JsonArrayConst colorArray = colorVariant;
                    if (colorArray.size() >= 3) {
                        uint8_t r = colorArray[0] | 0;
                        uint8_t g = colorArray[1] | 0;
                        uint8_t b = colorArray[2] | 0;
                        colors.push_back(color(r, g, b));
#ifdef ARDUINO
                        Serial.printf("  Color: RGB(%d,%d,%d)\n", r, g, b);
#endif
                    }
                }
            }
        }

        // Parse ranges array (optional)
        if (doc.containsKey("ranges")) {
#ifdef ARDUINO
            Serial.println("Found 'ranges' key in JSON");
#endif
            if (doc["ranges"].is<JsonArray>()) {
                JsonArrayConst rangesArray = doc["ranges"];
#ifdef ARDUINO
                Serial.printf("Ranges array size: %d\n", rangesArray.size());
#endif
                for (JsonVariantConst rangeVariant: rangesArray) {
                    float rangeValue = rangeVariant.as<float>();
                    ranges.push_back(rangeValue);
#ifdef ARDUINO
                    Serial.printf("  Range: %.2f\n", rangeValue);
#endif
                }
            } else {
#ifdef ARDUINO
                Serial.println("WARNING: 'ranges' exists but is not an array!");
#endif
            }
        } else {
#ifdef ARDUINO
            Serial.println("No 'ranges' key in JSON");
#endif
        }

        // If no colors parsed, use default Ukraine flag
        if (colors.empty()) {
#ifdef ARDUINO
            Serial.println("No colors parsed, using default Ukraine flag");
#endif
            colors.push_back(color(0, 87, 183)); // Blue
            colors.push_back(color(255, 215, 0)); // Yellow
        }

#ifdef ARDUINO
        Serial.printf("ShowFactory: Creating ColorRanges with %zu colors and %zu ranges\n",
                      colors.size(), ranges.size());
#endif
        return std::make_unique<Show::ColorRanges>(colors, ranges);
    });

    registerShow("TwoColorBlend", "Gradient between two colors", [](const StaticJsonDocument<512> &doc) {
        uint8_t r1 = doc["r1"] | 255; // Default to red
        uint8_t g1 = doc["g1"] | 0;
        uint8_t b1 = doc["b1"] | 0;
        uint8_t r2 = doc["r2"] | 0; // Default to blue
        uint8_t g2 = doc["g2"] | 0;
        uint8_t b2 = doc["b2"] | 255;
#ifdef ARDUINO
        Serial.printf("ShowFactory: Creating TwoColorBlend color1 RGB(%d,%d,%d) to color2 RGB(%d,%d,%d)\n",
                      r1, g1, b1, r2, g2, b2);
#endif
        return std::make_unique<Show::TwoColorBlend>(r1, g1, b1, r2, g2, b2);
    });

    registerShow("Starlight", "Twinkling stars effect", [](const StaticJsonDocument<512> &doc) {
        float probability = doc["probability"] | 0.1f;
        unsigned long length_ms = doc["length"] | 5000;
        unsigned long fade_ms = doc["fade"] | 1000;
        uint8_t r = doc["r"] | 255;
        uint8_t g = doc["g"] | 180;
        uint8_t b = doc["b"] | 50;
#ifdef ARDUINO
        Serial.printf("ShowFactory: Creating Starlight probability=%.2f, length=%lums, fade=%lums, RGB(%d,%d,%d)\n",
                      probability, length_ms, fade_ms, r, g, b);
#endif
        return std::make_unique<Show::Starlight>(probability, length_ms, fade_ms, r, g, b);
    });

    registerShow("Stroboscope", "Flashing strobe effect", [](const StaticJsonDocument<512> &doc) {
        uint8_t r = doc["r"] | 255;
        uint8_t g = doc["g"] | 255;
        uint8_t b = doc["b"] | 255;
        unsigned int on_cycles = doc["on_cycles"] | 1;
        unsigned int off_cycles = doc["off_cycles"] | 10;
#ifdef ARDUINO
        Serial.printf("ShowFactory: Creating Stroboscope RGB(%d,%d,%d), on=%u, off=%u\n",
                      r, g, b, on_cycles, off_cycles);
#endif
        return std::make_unique<Show::Stroboscope>(r, g, b, on_cycles, off_cycles);
    });

    registerShow("ColorRun", "Running colors", [](const StaticJsonDocument<512> &doc) {
        // ColorRun has no parameters yet
        return std::make_unique<Show::ColorRun>();
    });

    registerShow("Jump", "Jumping lights", [](const StaticJsonDocument<512> &doc) {
        // Jump has no parameters yet
        return std::make_unique<Show::Jump>();
    });

    registerShow("Rainbow", "Rainbow color cycle", [](const StaticJsonDocument<512> &doc) {
        // Rainbow has no parameters yet
        Serial.println("ShowFactory: Creating Rainbow");

        return std::make_unique<Show::Rainbow>();
    });

    registerShow("Wave", "Propagating wave with rainbow colors", [](const StaticJsonDocument<512> &doc) {
        float wave_speed = doc["wave_speed"] | 1.0f;
        float decay_rate = doc["decay_rate"] | 2.0f;
        float brightness_frequency = doc["brightness_frequency"] | 0.1f;
        float wavelength = doc["wavelength"] | 6.0f;
#ifdef ARDUINO
        Serial.printf("ShowFactory: Creating Wave speed=%.2f, decay=%.2f, freq=%.2f, wavelength=%.2f\n",
                      wave_speed, decay_rate, brightness_frequency, wavelength);
#endif
        return std::make_unique<Show::Wave>(wave_speed, decay_rate, brightness_frequency, wavelength);
    });

    registerShow("TheaterChase", "Marquee-style chase with rainbow colors", [](const StaticJsonDocument<512> &doc) {
        unsigned int num_steps_per_cycle = doc["num_steps_per_cycle"] | 21;
#ifdef ARDUINO
        Serial.printf("ShowFactory: Creating TheaterChase num_steps_per_cycle=%u\n",
                      num_steps_per_cycle);
#endif
        return std::make_unique<Show::TheaterChase>(num_steps_per_cycle);
    });

    registerShow("MorseCode", "Scrolling Morse code text display", [](const StaticJsonDocument<512> &doc) {
#ifdef ARDUINO
        String message = doc["message"] | "HELLO";
#else
        const char *message = "HELLO";
#endif
        float speed = doc["speed"] | 0.5f;
        unsigned int dot_length = doc["dot_length"] | 2;
        unsigned int dash_length = doc["dash_length"] | 4;
        unsigned int symbol_space = doc["symbol_space"] | 2;
        unsigned int letter_space = doc["letter_space"] | 3;
        unsigned int word_space = doc["word_space"] | 5;
#ifdef ARDUINO
        Serial.printf("ShowFactory: Creating MorseCode message=\"%s\", speed=%.2f, dot=%u, dash=%u\n",
                      message.c_str(), speed, dot_length, dash_length);
#endif
        return std::make_unique<Show::MorseCode>(message.c_str(), speed, dot_length, dash_length,
                                                 symbol_space, letter_space, word_space);
    });

    registerShow("Chaos", "Chaotic pattern", [](const StaticJsonDocument<512> &doc) {
        float Rmin = doc["Rmin"] | 2.95f;
        float Rmax = doc["Rmax"] | 4.0f;
        float Rdelta = doc["Rdelta"] | 0.0002f;
#ifdef ARDUINO
        Serial.printf("ShowFactory: Creating Chaos Rmin=%.4f, Rmax=%.4f, Rdelta=%.6f\n",
                      Rmin, Rmax, Rdelta);
#endif
        return std::make_unique<Show::Chaos>(Rmin, Rmax, Rdelta);
    });

    registerShow("Mandelbrot", "Mandelbrot fractal zoom", [](const StaticJsonDocument<512> &doc) {
        float Cre0 = doc["Cre0"] | -1.05f;
        float Cim0 = doc["Cim0"] | -0.3616f;
        float Cim1 = doc["Cim1"] | -0.3156f;
        unsigned int scale = doc["scale"] | 5;
        unsigned int max_iterations = doc["max_iterations"] | 50;
        unsigned int color_scale = doc["color_scale"] | 10;
#ifdef ARDUINO
        Serial.printf(
            "ShowFactory: Creating Mandelbrot Cre0=%.4f, Cim0=%.4f, Cim1=%.4f, scale=%u, max_iter=%u, color_scale=%u\n",
            Cre0, Cim0, Cim1, scale, max_iterations, color_scale);
#endif
        return std::make_unique<Show::Mandelbrot>(Cre0, Cim0, Cim1, scale, max_iterations, color_scale);
    });
}

void ShowFactory::registerShow(const char *name, const char *description, ShowConstructor &&constructor) {
    showConstructors[name] = std::move(constructor);
    showList.push_back({name, description});
}

std::unique_ptr<Show::Show> ShowFactory::createShow(const char *name) {
    return createShow(name, "{}"); // Default to empty params
}

std::unique_ptr<Show::Show> ShowFactory::createShow(const char *name, const char *paramsJson) {
    // Check if show exists
    auto it = showConstructors.find(name);
    if (it == showConstructors.end()) {
#ifdef ARDUINO
        Serial.printf("ShowFactory: show %s not found", name);
#endif
        return {};
    }

#ifdef ARDUINO
    // Parse JSON parameters (increased size for ColorRanges with many colors)
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, paramsJson);

    // If JSON parsing fails, log warning and use empty document (will use defaults)
    if (error) {
        Serial.print("ShowFactory: Failed to parse params for ");
        Serial.print(name);
        Serial.print(": ");
        Serial.println(error.c_str());
        Serial.println("Using default parameters");
        doc.clear(); // Empty document will trigger all defaults via | operator
    }

    // Call the stored factory function with the parsed (or empty) JSON document
    return it->second(doc);
#else
    // Non-Arduino builds: use empty document (defaults)
    StaticJsonDocument<512> doc;
    return std::move(it->second(doc));
#endif
}

const std::vector<ShowFactory::ShowInfo> &ShowFactory::listShows() const {
    return showList;
}

bool ShowFactory::hasShow(const char *name) const {
    return showConstructors.find(name) != showConstructors.end();
}
