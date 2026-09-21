#include <iostream>
#include <limits>
#include <memory>
#include <ostream>

#include "unity.h"
#include "color.h"
#include "support/color.h"

void setUp() {
}

void tearDown() {
}

void test_color_extraction() {
    Strip::Color test_color = 0x123456;

    TEST_ASSERT_EQUAL_UINT8(0x12, red(test_color));
    TEST_ASSERT_EQUAL_UINT8(0x34, green(test_color));
    TEST_ASSERT_EQUAL_UINT8(0x56, blue(test_color));
}

void test_color_construction() {
    Strip::Color result = color(0x12, 0x34, 0x56);
    TEST_ASSERT_EQUAL_UINT32(0x123456, result);
}

void test_color_red_component() {
    Strip::Color pure_red = 0xFF0000;
    TEST_ASSERT_EQUAL_UINT8(0xFF, red(pure_red));
    TEST_ASSERT_EQUAL_UINT8(0x00, green(pure_red));
    TEST_ASSERT_EQUAL_UINT8(0x00, blue(pure_red));
}

void test_color_green_component() {
    Strip::Color pure_green = 0x00FF00;
    TEST_ASSERT_EQUAL_UINT8(0x00, red(pure_green));
    TEST_ASSERT_EQUAL_UINT8(0xFF, green(pure_green));
    TEST_ASSERT_EQUAL_UINT8(0x00, blue(pure_green));
}

void test_color_blue_component() {
    Strip::Color pure_blue = 0x0000FF;
    TEST_ASSERT_EQUAL_UINT8(0x00, red(pure_blue));
    TEST_ASSERT_EQUAL_UINT8(0x00, green(pure_blue));
    TEST_ASSERT_EQUAL_UINT8(0xFF, blue(pure_blue));
}

void test_black_body_color() {
    // temp=0.0: brightness=0, result is black
    TEST_ASSERT_EQUAL_UINT32(0x000000, Support::Color::black_body_color(0.0f));
    // temp=0.2: 1000K, brightness=0.667 → (169, 44, 0)
    TEST_ASSERT_EQUAL_UINT32(0xA92C00, Support::Color::black_body_color(0.20f));
    // temp=0.8: 1600K, green=114, brightness=1.0 → (255, 114, 0)
    TEST_ASSERT_EQUAL_UINT32(0xFF7200, Support::Color::black_body_color(0.80f));
    // temp=1.0: 1800K, green=126, brightness=1.0 → (255, 126, 0)
    TEST_ASSERT_EQUAL_UINT32(0xFF7E00, Support::Color::black_body_color(1.0f));
    // Values > 1.0 are clamped to 1.0
    TEST_ASSERT_EQUAL_UINT32(0xFF7E00, Support::Color::black_body_color(5.0f));
    TEST_ASSERT_EQUAL_UINT32(0xFF7E00, Support::Color::black_body_color(100.0f));
}

// Reference implementation of the prior unsigned-char wheel body. The new
// float wheel must produce the same Strip::Color at every integer input in
// [0, 254] as this function does. This is the byte-identical-at-integers
// guarantee that keeps the three integer callers (Chaos, Mandelbrot,
// MorseCode) byte-identical and lets the three float callers (Rainbow, Wave,
// TheaterChase) simplify to a direct wheel() call without behaviour change
// for any prior configuration.
static Strip::Color reference_wheel_uint8(unsigned char h) {
    if (h > 254) h = 254;
    uint16_t pos = static_cast<uint16_t>(h) * 6;
    if (h <= 42)  return (255u << 16) | (static_cast<unsigned>(pos) << 8);
    if (h <= 84)  return (static_cast<unsigned>(510 - pos) << 16) | (255u << 8);
    if (h <= 127) return (255u << 8) | static_cast<unsigned>(pos - 510);
    if (h <= 169) return (static_cast<unsigned>(1020 - pos) << 8) | 255u;
    if (h <= 212) return (static_cast<unsigned>(pos - 1020) << 16) | 255u;
    return (255u << 16) | static_cast<unsigned>(1530 - pos);
}

void test_wheel_byte_identical_at_integer_inputs() {
    for (int h = 0; h <= 254; h++) {
        Strip::Color expected = reference_wheel_uint8(static_cast<unsigned char>(h));
        Strip::Color actual = wheel(static_cast<float>(h));
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(expected, actual, ("hue=" + std::to_string(h)).c_str());
    }
}

void test_wheel_pure_red_green_blue() {
    TEST_ASSERT_EQUAL_UINT32(0xFF0000, wheel(0.0f));
    TEST_ASSERT_EQUAL_UINT32(0x00FF00, wheel(85.0f));
    TEST_ASSERT_EQUAL_UINT32(0x0000FF, wheel(170.0f));
}

void test_wheel_pure_yellow_cyan_magenta() {
    // Mid-sector hues — unreachable with the prior unsigned-char input because
    // every integer index sat one channel slightly off full intensity.
    TEST_ASSERT_EQUAL_UINT32(0xFFFF00, wheel(42.5f));
    TEST_ASSERT_EQUAL_UINT32(0x00FFFF, wheel(127.5f));
    TEST_ASSERT_EQUAL_UINT32(0xFF00FF, wheel(212.5f));
}

void test_wheel_wraps_at_full_revolutions() {
    TEST_ASSERT_EQUAL_UINT32(wheel(0.0f), wheel(255.0f));
    TEST_ASSERT_EQUAL_UINT32(wheel(0.0f), wheel(510.0f));
}

void test_wheel_wraps_negative_inputs() {
    TEST_ASSERT_EQUAL_UINT32(wheel(254.0f), wheel(-1.0f));
    TEST_ASSERT_EQUAL_UINT32(wheel(0.0f), wheel(-255.0f));
}

void test_wheel_clamps_sub_degree_hue() {
    // After wrap, h ∈ [254, 255) is clamped to 254 so the result matches the
    // existing wheel(254) output rather than introducing a never-before-seen
    // value in the tiny sliver between 254 and 255.
    TEST_ASSERT_EQUAL_UINT32(wheel(254.0f), wheel(254.7f));
}

void test_wheel_nan_returns_black() {
    TEST_ASSERT_EQUAL_UINT32(0x000000, wheel(std::numeric_limits<float>::quiet_NaN()));
}

void test_wheel_infinities_return_black() {
    TEST_ASSERT_EQUAL_UINT32(0x000000, wheel(std::numeric_limits<float>::infinity()));
    TEST_ASSERT_EQUAL_UINT32(0x000000, wheel(-std::numeric_limits<float>::infinity()));
}

int runUnityTests() {
    UNITY_BEGIN();
    RUN_TEST(test_color_extraction);
    RUN_TEST(test_color_construction);
    RUN_TEST(test_color_red_component);
    RUN_TEST(test_color_green_component);
    RUN_TEST(test_color_blue_component);
    RUN_TEST(test_black_body_color);
    RUN_TEST(test_wheel_byte_identical_at_integer_inputs);
    RUN_TEST(test_wheel_pure_red_green_blue);
    RUN_TEST(test_wheel_pure_yellow_cyan_magenta);
    RUN_TEST(test_wheel_wraps_at_full_revolutions);
    RUN_TEST(test_wheel_wraps_negative_inputs);
    RUN_TEST(test_wheel_clamps_sub_degree_hue);
    RUN_TEST(test_wheel_nan_returns_black);
    RUN_TEST(test_wheel_infinities_return_black);
    return UNITY_END();
}

int main() {
    return runUnityTests();
}
