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
    registerShow("Solid", "Solid color (default: white)", []() {
        return std::unique_ptr<Show::Show>(new Show::Solid(255, 255, 255)); // Default to white
    });

    registerShow("ColorRanges", "Solid color sections (flags, patterns)", []() {
        // Default: Ukraine flag (blue and yellow)
        std::vector<Strip::Color> colors = {
            color(0, 87, 183),    // Blue
            color(255, 215, 0)    // Yellow
        };
        return std::unique_ptr<Show::Show>(new Show::ColorRanges(colors));
    });

    registerShow("TwoColorBlend", "Gradient between two colors", []() {
        return std::unique_ptr<Show::Show>(new Show::TwoColorBlend(255, 0, 0, 0, 0, 255)); // Default: red to blue
    });

    registerShow("Starlight", "Twinkling stars effect", []() {
        return std::unique_ptr<Show::Show>(new Show::Starlight()); // Default: 0.1 probability, 5s length, 1s fade, warm white
    });

    registerShow("Stroboscope", "Flashing strobe effect", []() {
        return std::unique_ptr<Show::Show>(new Show::Stroboscope()); // Default: white, 1 on, 10 off
    });

    registerShow("ColorRun", "Running colors", []() {
        return std::unique_ptr<Show::Show>(new Show::ColorRun());
    });

    registerShow("Jump", "Jumping lights", []() {
        return std::unique_ptr<Show::Show>(new Show::Jump());
    });

    registerShow("Rainbow", "Rainbow color cycle", []() {
        return std::unique_ptr<Show::Show>(new Show::Rainbow());
    });

    registerShow("Wave", "Propagating wave with rainbow colors", []() {
        return std::unique_ptr<Show::Show>(new Show::Wave()); // Default: 1.0 speed, 2.0 decay, 0.1 freq, 6.0 wavelength
    });

    registerShow("TheaterChase", "Marquee-style chase with rainbow colors", []() {
        return std::unique_ptr<Show::Show>(new Show::TheaterChase()); // Default: 21 steps per cycle
    });

    registerShow("MorseCode", "Scrolling Morse code text display", []() {
        return std::unique_ptr<Show::Show>(new Show::MorseCode()); // Default: "HELLO", 0.5 speed
    });

    registerShow("Chaos", "Chaotic pattern", []() {
        return std::unique_ptr<Show::Show>(new Show::Chaos(2.95f, 4.0f, 0.0002f));
    });

    registerShow("Mandelbrot", "Mandelbrot fractal zoom", []() {
        return std::unique_ptr<Show::Show>(new Show::Mandelbrot(-1.05, -0.3616, -0.3156, 5, 50, 10));
    });
}

void ShowFactory::registerShow(const char* name, const char* description, ShowConstructor constructor) {
    showConstructors[name] = constructor;
    showList.push_back({name, description});
}

std::unique_ptr<Show::Show> &&ShowFactory::createShow(const char *name) {
    return createShow(name, "{}"); // Default to empty params
}

std::unique_ptr<Show::Show> &&ShowFactory::createShow(const char *name, const char *paramsJson) {
    // First check if show exists
    auto it = showConstructors.find(name);
    if (it == showConstructors.end()) {
        return std::move(std::unique_ptr<Show::Show>());
    }

#ifdef ARDUINO
    // Parse JSON parameters (increased size for ColorRanges with many colors)
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, paramsJson);

    // If JSON parsing fails, use defaults
    if (error) {
        Serial.print("ShowFactory: Failed to parse params for ");
        Serial.print(name);
        Serial.print(": ");
        Serial.println(error.c_str());
        return std::move(it->second()); // Use default constructor
    }

    // Create show with parameters based on show type
    if (strcmp(name, "Solid") == 0) {
        // Parse color parameters: {"r":255, "g":0, "b":0}
        uint8_t r = doc["r"] | 255;  // Default to white
        uint8_t g = doc["g"] | 255;
        uint8_t b = doc["b"] | 255;
        Serial.printf("ShowFactory: Creating Solid with color RGB(%d,%d,%d)\n", r, g, b);
        return std::unique_ptr<Show::Show>(new Show::Solid(r, g, b));
    }
    else if (strcmp(name, "Mandelbrot") == 0) {
        // Parse Mandelbrot parameters: {"Cre0":-1.05, "Cim0":-0.3616, "Cim1":-0.3156, "scale":5, "max_iterations":50, "color_scale":10}
        float Cre0 = doc["Cre0"] | -1.05f;
        float Cim0 = doc["Cim0"] | -0.3616f;
        float Cim1 = doc["Cim1"] | -0.3156f;
        unsigned int scale = doc["scale"] | 5;
        unsigned int max_iterations = doc["max_iterations"] | 50;
        unsigned int color_scale = doc["color_scale"] | 10;
        Serial.printf("ShowFactory: Creating Mandelbrot Cre0=%.4f, Cim0=%.4f, Cim1=%.4f, scale=%u, max_iter=%u, color_scale=%u\n",
                     Cre0, Cim0, Cim1, scale, max_iterations, color_scale);
        return std::unique_ptr<Show::Show>(new Show::Mandelbrot(Cre0, Cim0, Cim1, scale, max_iterations, color_scale));
    }
    else if (strcmp(name, "Chaos") == 0) {
        // Parse Chaos parameters: {"Rmin":2.95, "Rmax":4.0, "Rdelta":0.0002}
        float Rmin = doc["Rmin"] | 2.95f;
        float Rmax = doc["Rmax"] | 4.0f;
        float Rdelta = doc["Rdelta"] | 0.0002f;
        Serial.printf("ShowFactory: Creating Chaos Rmin=%.4f, Rmax=%.4f, Rdelta=%.6f\n",
                     Rmin, Rmax, Rdelta);
        return std::unique_ptr<Show::Show>(new Show::Chaos(Rmin, Rmax, Rdelta));
    }
    else if (strcmp(name, "TwoColorBlend") == 0) {
        // Parse TwoColorBlend parameters: {"r1":255, "g1":0, "b1":0, "r2":0, "g2":0, "b2":255}
        uint8_t r1 = doc["r1"] | 255;  // Default to red
        uint8_t g1 = doc["g1"] | 0;
        uint8_t b1 = doc["b1"] | 0;
        uint8_t r2 = doc["r2"] | 0;    // Default to blue
        uint8_t g2 = doc["g2"] | 0;
        uint8_t b2 = doc["b2"] | 255;
        Serial.printf("ShowFactory: Creating TwoColorBlend color1 RGB(%d,%d,%d) to color2 RGB(%d,%d,%d)\n",
                     r1, g1, b1, r2, g2, b2);
        return std::unique_ptr<Show::Show>(new Show::TwoColorBlend(r1, g1, b1, r2, g2, b2));
    }
    else if (strcmp(name, "ColorRanges") == 0) {
        // Parse ColorRanges parameters: {"colors":[[r1,g1,b1],[r2,g2,b2],...], "ranges":[33.3, 66.6]}
        std::vector<Strip::Color> colors;
        std::vector<float> ranges;

        Serial.println("ShowFactory: Parsing ColorRanges parameters");
        Serial.print("JSON: ");
        serializeJson(doc, Serial);
        Serial.println();

        // Parse colors array
        if (doc.containsKey("colors") && doc["colors"].is<JsonArray>()) {
            JsonArray colorsArray = doc["colors"];
            Serial.printf("Found %d colors in array\n", colorsArray.size());
            for (JsonVariant colorVariant : colorsArray) {
                if (colorVariant.is<JsonArray>()) {
                    JsonArray colorArray = colorVariant;
                    if (colorArray.size() >= 3) {
                        uint8_t r = colorArray[0] | 0;
                        uint8_t g = colorArray[1] | 0;
                        uint8_t b = colorArray[2] | 0;
                        colors.push_back(color(r, g, b));
                        Serial.printf("  Color: RGB(%d,%d,%d)\n", r, g, b);
                    }
                }
            }
        }

        // Parse ranges array (optional)
        if (doc.containsKey("ranges")) {
            Serial.println("Found 'ranges' key in JSON");
            if (doc["ranges"].is<JsonArray>()) {
                JsonArray rangesArray = doc["ranges"];
                Serial.printf("Ranges array size: %d\n", rangesArray.size());
                for (JsonVariant rangeVariant : rangesArray) {
                    float rangeValue = rangeVariant.as<float>();
                    ranges.push_back(rangeValue);
                    Serial.printf("  Range: %.2f\n", rangeValue);
                }
            } else {
                Serial.println("WARNING: 'ranges' exists but is not an array!");
            }
        } else {
            Serial.println("No 'ranges' key in JSON");
        }

        // If no colors parsed, use default Ukraine flag
        if (colors.empty()) {
            Serial.println("No colors parsed, using default Ukraine flag");
            colors.push_back(color(0, 87, 183));   // Blue
            colors.push_back(color(255, 215, 0));  // Yellow
        }

        Serial.printf("ShowFactory: Creating ColorRanges with %zu colors and %zu ranges\n",
                     colors.size(), ranges.size());
        return std::unique_ptr<Show::Show>(new Show::ColorRanges(colors, ranges));
    }
    else if (strcmp(name, "Starlight") == 0) {
        // Parse Starlight parameters: {"probability":0.1, "length":5000, "fade":1000, "r":255, "g":180, "b":50}
        float probability = doc["probability"] | 0.1f;
        unsigned long length_ms = doc["length"] | 5000;
        unsigned long fade_ms = doc["fade"] | 1000;
        uint8_t r = doc["r"] | 255;
        uint8_t g = doc["g"] | 180;
        uint8_t b = doc["b"] | 50;

        Serial.printf("ShowFactory: Creating Starlight probability=%.2f, length=%lums, fade=%lums, RGB(%d,%d,%d)\n",
                     probability, length_ms, fade_ms, r, g, b);
        return std::unique_ptr<Show::Show>(new Show::Starlight(probability, length_ms, fade_ms, r, g, b));
    }
    else if (strcmp(name, "Wave") == 0) {
        // Parse Wave parameters: {"wave_speed":1.0, "decay_rate":2.0, "brightness_frequency":0.1, "wavelength":6.0}
        float wave_speed = doc["wave_speed"] | 1.0f;
        float decay_rate = doc["decay_rate"] | 2.0f;
        float brightness_frequency = doc["brightness_frequency"] | 0.1f;
        float wavelength = doc["wavelength"] | 6.0f;

        Serial.printf("ShowFactory: Creating Wave speed=%.2f, decay=%.2f, freq=%.2f, wavelength=%.2f\n",
                     wave_speed, decay_rate, brightness_frequency, wavelength);
        return std::unique_ptr<Show::Show>(new Show::Wave(wave_speed, decay_rate, brightness_frequency, wavelength));
    }
    else if (strcmp(name, "MorseCode") == 0) {
        // Parse MorseCode parameters: {"message":"HELLO", "speed":0.5, "dot_length":2, "dash_length":4, "symbol_space":2, "letter_space":3, "word_space":5}
        String message = doc["message"] | "HELLO";
        float speed = doc["speed"] | 0.5f;
        unsigned int dot_length = doc["dot_length"] | 2;
        unsigned int dash_length = doc["dash_length"] | 4;
        unsigned int symbol_space = doc["symbol_space"] | 2;
        unsigned int letter_space = doc["letter_space"] | 3;
        unsigned int word_space = doc["word_space"] | 5;

        Serial.printf("ShowFactory: Creating MorseCode message=\"%s\", speed=%.2f, dot=%u, dash=%u\n",
                     message.c_str(), speed, dot_length, dash_length);
        return std::unique_ptr<Show::Show>(new Show::MorseCode(message.c_str(), speed, dot_length, dash_length,
                                   symbol_space, letter_space, word_space));
    }
    else if (strcmp(name, "TheaterChase") == 0) {
        // Parse TheaterChase parameters: {"num_steps_per_cycle":21}
        unsigned int num_steps_per_cycle = doc["num_steps_per_cycle"] | 21;

        Serial.printf("ShowFactory: Creating TheaterChase num_steps_per_cycle=%u\n",
                     num_steps_per_cycle);
        return std::unique_ptr<Show::Show>(new Show::TheaterChase(num_steps_per_cycle));
    }
    else if (strcmp(name, "Stroboscope") == 0) {
        // Parse Stroboscope parameters: {"r":255, "g":255, "b":255, "on_cycles":1, "off_cycles":10}
        uint8_t r = doc["r"] | 255;
        uint8_t g = doc["g"] | 255;
        uint8_t b = doc["b"] | 255;
        unsigned int on_cycles = doc["on_cycles"] | 1;
        unsigned int off_cycles = doc["off_cycles"] | 10;

        Serial.printf("ShowFactory: Creating Stroboscope RGB(%d,%d,%d), on=%u, off=%u\n",
                     r, g, b, on_cycles, off_cycles);
        return std::unique_ptr<Show::Show>(new Show::Stroboscope(r, g, b, on_cycles, off_cycles));
    }
    // Other shows don't support parameters yet, use default constructor
    else {
        return it->second();
    }
#else
    // Non-Arduino builds: use default constructor
    return it->second();
#endif
}

const std::vector<ShowFactory::ShowInfo>& ShowFactory::listShows() const {
    return showList;
}

bool ShowFactory::hasShow(const char* name) const {
    return showConstructors.find(name) != showConstructors.end();
}
