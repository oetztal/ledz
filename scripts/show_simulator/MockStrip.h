#ifndef SHOW_SIMULATOR_MOCKSTRIP_H
#define SHOW_SIMULATOR_MOCKSTRIP_H

#include <vector>

#include "strip/Strip.h"

// Vector-backed strip used by the host-side show simulator. Mirrors the
// semantics of test/MockStrip.h but lives under scripts/ because PlatformIO's
// test harness filters test/ out of non-test binaries; the simulator binary
// therefore needs its own copy rather than depending on the test mock.
class MockStrip : public Strip::Strip {
private:
    std::vector<::Strip::Color> pixels;
    ::Strip::PixelIndex pixel_count;

public:
    explicit MockStrip(::Strip::PixelIndex count) : pixel_count(count) {
        pixels.resize(count, 0x000000);
    }

    void fill(::Strip::Color c) override {
        for (::Strip::PixelIndex i = 0; i < pixel_count; i++) {
            pixels[i] = c;
        }
    }

    void setPixelColor(::Strip::PixelIndex pixel_index, ::Strip::Color col) override {
        if (pixel_index >= 0 && pixel_index < pixel_count) {
            pixels[pixel_index] = col;
        }
    }

    ::Strip::Color getPixelColor(::Strip::PixelIndex pixel_index) const override {
        if (pixel_index >= 0 && pixel_index < pixel_count) {
            return pixels[pixel_index];
        }
        return 0;
    }

    ::Strip::PixelIndex length() const override {
        return pixel_count;
    }

    void show() override {
        // No-op: simulator reads back via getPixelColor, never sends to a strip.
    }

    void setBrightness(uint8_t) override {
        // No-op: brightness scaling happens on the device, not the simulator.
    }

    [[nodiscard]] ::Strip::Brightness getBrightness() const override {
        return 255;
    }

    // Direct access for the simulator when emitting the raw RGB stream.
    const std::vector<::Strip::Color> &buffer() const { return pixels; }
};

#endif // SHOW_SIMULATOR_MOCKSTRIP_H
