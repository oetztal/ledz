//
// Created by Claude Code on 03.01.26.
//

#include "ShowFactory.h"
#include "show/Rainbow.h"
#include "show/ColorRun.h"
#include "show/Mandelbrot.h"
#include "show/Chaos.h"
#include "show/Jump.h"

ShowFactory::ShowFactory() : showConstructors(strLess) {
    // Register all available shows
    registerShow("Rainbow", "Rainbow color cycle", []() {
        return new Show::Rainbow();
    });

    registerShow("ColorRun", "Running colors", []() {
        return new Show::ColorRun();
    });

    registerShow("Mandelbrot", "Mandelbrot fractal zoom", []() {
        return new Show::Mandelbrot(-1.05, -0.3616, -0.3156);
    });

    registerShow("Chaos", "Chaotic pattern", []() {
        return new Show::Chaos();
    });

    registerShow("Jump", "Jumping lights", []() {
        return new Show::Jump();
    });
}

void ShowFactory::registerShow(const char* name, const char* description, ShowConstructor constructor) {
    showConstructors[name] = constructor;
    showList.push_back({name, description});
}

Show::Show* ShowFactory::createShow(const char* name) {
    auto it = showConstructors.find(name);
    if (it != showConstructors.end()) {
        return it->second();
    }
    return nullptr;
}

const std::vector<ShowFactory::ShowInfo>& ShowFactory::listShows() const {
    return showList;
}

bool ShowFactory::hasShow(const char* name) const {
    return showConstructors.find(name) != showConstructors.end();
}
