//
// Created by Claude Code on 03.01.26.
//

#include "ShowFactory.h"
#include "show/Rainbow.h"
#include "show/ColorRun.h"
#include "show/Mandelbrot.h"
#include "show/Chaos.h"
#include "show/Jump.h"
#include "show/Solid.h"

#ifdef ARDUINO
#include <ArduinoJson.h>
#endif

ShowFactory::ShowFactory() : showConstructors(strLess) {
    // Register all available shows
    registerShow("Rainbow", "Rainbow color cycle", []() {
        return new Show::Rainbow();
    });

    registerShow("ColorRun", "Running colors", []() {
        return new Show::ColorRun();
    });

    registerShow("Mandelbrot", "Mandelbrot fractal zoom", []() {
        return new Show::Mandelbrot(-1.05, -0.3616, -0.3156, 5, 50, 10);
    });

    registerShow("Chaos", "Chaotic pattern", []() {
        return new Show::Chaos();
    });

    registerShow("Jump", "Jumping lights", []() {
        return new Show::Jump();
    });

    registerShow("Solid", "Solid color (default: white)", []() {
        return new Show::Solid(255, 255, 255); // Default to white
    });
}

void ShowFactory::registerShow(const char* name, const char* description, ShowConstructor constructor) {
    showConstructors[name] = constructor;
    showList.push_back({name, description});
}

Show::Show* ShowFactory::createShow(const char* name) {
    return createShow(name, "{}"); // Default to empty params
}

Show::Show* ShowFactory::createShow(const char* name, const char* paramsJson) {
    // First check if show exists
    auto it = showConstructors.find(name);
    if (it == showConstructors.end()) {
        return nullptr;
    }

#ifdef ARDUINO
    // Parse JSON parameters
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, paramsJson);

    // If JSON parsing fails, use defaults
    if (error) {
        Serial.print("ShowFactory: Failed to parse params for ");
        Serial.print(name);
        Serial.print(": ");
        Serial.println(error.c_str());
        return it->second(); // Use default constructor
    }

    // Create show with parameters based on show type
    if (strcmp(name, "Solid") == 0) {
        // Parse color parameters: {"r":255, "g":0, "b":0}
        uint8_t r = doc["r"] | 255;  // Default to white
        uint8_t g = doc["g"] | 255;
        uint8_t b = doc["b"] | 255;
        Serial.printf("ShowFactory: Creating Solid with color RGB(%d,%d,%d)\n", r, g, b);
        return new Show::Solid(r, g, b);
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
        return new Show::Mandelbrot(Cre0, Cim0, Cim1, scale, max_iterations, color_scale);
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
