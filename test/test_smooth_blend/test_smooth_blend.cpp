#include "unity.h"
#include "support/SmoothBlend.h"
#include "color.h"

// Mock strip for testing
class MockStrip : public Strip::Strip {
private:
    std::vector<::Strip::Color> pixels;
    ::Strip::PixelIndex pixel_count;

public:
    MockStrip(::Strip::PixelIndex count) : pixel_count(count) {
        pixels.resize(count, 0x000000);
    }

    void fill(::Strip::Color c) const override {
        // Mock implementation
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
        // Mock implementation
    }
};

MockStrip* mock_strip;

void setUp() {
    mock_strip = new MockStrip(10);
}

void tearDown() {
    delete mock_strip;
}

void test_smooth_blend_single_color() {
    // Set initial colors
    for (int i = 0; i < 10; i++) {
        mock_strip->setPixelColor(i, 0xFF0000); // Red
    }

    // Create blend to blue with 2 second duration (default)
    Support::SmoothBlend blend(*mock_strip, 0x0000FF);

    // Initially should not be complete
    TEST_ASSERT_FALSE(blend.isComplete());

    // Do a few steps - the blend should still be in progress
    bool still_running = blend.step();
    TEST_ASSERT_TRUE(still_running);

    still_running = blend.step();
    TEST_ASSERT_TRUE(still_running);
}

void test_smooth_blend_multiple_colors() {
    // Set initial colors to red
    for (int i = 0; i < 10; i++) {
        mock_strip->setPixelColor(i, 0xFF0000);
    }

    // Create target colors (alternating blue and green)
    std::vector<::Strip::Color> targets;
    for (int i = 0; i < 10; i++) {
        targets.push_back(i % 2 == 0 ? 0x0000FF : 0x00FF00);
    }

    Support::SmoothBlend blend(*mock_strip, targets);

    // Initially should not be complete
    TEST_ASSERT_FALSE(blend.isComplete());

    // Do a few steps
    bool still_running = blend.step();
    TEST_ASSERT_TRUE(still_running);
}

int runUnityTests() {
    UNITY_BEGIN();
    RUN_TEST(test_smooth_blend_single_color);
    RUN_TEST(test_smooth_blend_multiple_colors);
    return UNITY_END();
}

int main() {
    return runUnityTests();
}
