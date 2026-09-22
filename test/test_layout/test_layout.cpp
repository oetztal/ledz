#include "unity.h"
#include "../MockStrip.h"
#include "strip/Layout.h"

static constexpr Strip::Color BLACK = 0x000000;
static constexpr Strip::Color WHITE = 0xFFFFFF;
static constexpr Strip::Color RED = 0xFF0000;
static constexpr Strip::Color GREEN = 0x00FF00;
static constexpr Strip::Color BLUE = 0x0000FF;

void setUp() {
}

void tearDown() {
}

// Plain strip: no reverse, no mirror, no dead LEDs. Every logical index maps
// 1:1 onto the physical strip.
void test_plain_no_dead_is_identity() {
    MockStrip phys(5);
    Strip::Layout layout(phys, false, false, 0);

    TEST_ASSERT_EQUAL_INT16(5, layout.length());

    layout.setPixelColor(0, RED);
    layout.setPixelColor(4, GREEN);
    TEST_ASSERT_EQUAL_HEX32(RED, phys.getPixelColor(0));
    TEST_ASSERT_EQUAL_HEX32(GREEN, phys.getPixelColor(4));
    TEST_ASSERT_EQUAL_HEX32(RED, layout.getPixelColor(0));
    TEST_ASSERT_EQUAL_HEX32(GREEN, layout.getPixelColor(4));

    // fill() with dead_leds == 0 takes the early-return path in turnOffDeadLeds.
    layout.fill(BLUE);
    for (Strip::PixelIndex i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL_HEX32(BLUE, phys.getPixelColor(i));
    }
}

// reverse maps logical index 0 onto the physical far end.
void test_reverse_maps_index_from_far_end() {
    MockStrip phys(5);
    Strip::Layout layout(phys, true, false, 0);

    TEST_ASSERT_EQUAL_INT16(5, layout.length());

    layout.setPixelColor(0, RED);
    TEST_ASSERT_EQUAL_HEX32(RED, phys.getPixelColor(4));
    TEST_ASSERT_EQUAL_HEX32(BLACK, phys.getPixelColor(0));
    TEST_ASSERT_EQUAL_HEX32(RED, layout.getPixelColor(0));

    layout.setPixelColor(4, BLUE);
    TEST_ASSERT_EQUAL_HEX32(BLUE, phys.getPixelColor(0));
}

// Positive dead_leds on a plain strip hides LEDs at the beginning and shifts
// every logical index past them.
void test_plain_positive_dead_clears_beginning() {
    MockStrip phys(10);
    phys.fill(WHITE);
    Strip::Layout layout(phys, false, false, 2);

    TEST_ASSERT_EQUAL_INT16(8, layout.length());
    // Constructor clears the dead LEDs at the start.
    TEST_ASSERT_EQUAL_HEX32(BLACK, phys.getPixelColor(0));
    TEST_ASSERT_EQUAL_HEX32(BLACK, phys.getPixelColor(1));

    TEST_ASSERT_EQUAL_HEX32(WHITE, phys.getPixelColor(2));
    TEST_ASSERT_EQUAL_HEX32(WHITE, phys.getPixelColor(9));
    TEST_ASSERT_EQUAL_HEX32(WHITE, layout.getPixelColor(0));

    layout.setPixelColor(0, RED);
    TEST_ASSERT_EQUAL_HEX32(RED, phys.getPixelColor(2));
    TEST_ASSERT_EQUAL_HEX32(BLACK, phys.getPixelColor(0));

    layout.fill(BLUE);
    TEST_ASSERT_EQUAL_HEX32(BLACK, phys.getPixelColor(0));
    TEST_ASSERT_EQUAL_HEX32(BLACK, phys.getPixelColor(1));
    TEST_ASSERT_EQUAL_HEX32(BLUE, phys.getPixelColor(2));
    TEST_ASSERT_EQUAL_HEX32(BLUE, phys.getPixelColor(9));
}

// Negative dead_leds on a plain strip hides LEDs at the end; logical indices
// stay aligned with the physical start.
void test_plain_negative_dead_clears_end() {
    MockStrip phys(10);
    phys.fill(WHITE);
    Strip::Layout layout(phys, false, false, -3);

    TEST_ASSERT_EQUAL_INT16(7, layout.length());
    TEST_ASSERT_EQUAL_HEX32(BLACK, phys.getPixelColor(7));
    TEST_ASSERT_EQUAL_HEX32(BLACK, phys.getPixelColor(8));
    TEST_ASSERT_EQUAL_HEX32(BLACK, phys.getPixelColor(9));

    TEST_ASSERT_EQUAL_HEX32(WHITE, phys.getPixelColor(0));
    TEST_ASSERT_EQUAL_HEX32(WHITE, phys.getPixelColor(6));
    TEST_ASSERT_EQUAL_HEX32(WHITE, layout.getPixelColor(6));

    layout.setPixelColor(6, RED);
    TEST_ASSERT_EQUAL_HEX32(RED, phys.getPixelColor(6));

    layout.fill(BLUE);
    TEST_ASSERT_EQUAL_HEX32(BLUE, phys.getPixelColor(0));
    TEST_ASSERT_EQUAL_HEX32(BLUE, phys.getPixelColor(6));
    TEST_ASSERT_EQUAL_HEX32(BLACK, phys.getPixelColor(7));
    TEST_ASSERT_EQUAL_HEX32(BLACK, phys.getPixelColor(8));
    TEST_ASSERT_EQUAL_HEX32(BLACK, phys.getPixelColor(9));
}

// A mirrored strip writes each logical pixel to both halves; positive dead
// LEDs sit in the middle between the two halves.
void test_mirror_positive_dead_clears_middle() {
    MockStrip phys(20);
    phys.fill(WHITE);
    Strip::Layout layout(phys, false, true, 2);

    TEST_ASSERT_EQUAL_INT16(9, layout.length());
    TEST_ASSERT_EQUAL_HEX32(BLACK, phys.getPixelColor(9));
    TEST_ASSERT_EQUAL_HEX32(BLACK, phys.getPixelColor(10));
    TEST_ASSERT_EQUAL_HEX32(WHITE, phys.getPixelColor(8));
    TEST_ASSERT_EQUAL_HEX32(WHITE, phys.getPixelColor(11));

    layout.setPixelColor(0, RED);
    TEST_ASSERT_EQUAL_HEX32(RED, phys.getPixelColor(0));
    TEST_ASSERT_EQUAL_HEX32(RED, phys.getPixelColor(19));
    TEST_ASSERT_EQUAL_HEX32(RED, layout.getPixelColor(0));

    layout.setPixelColor(8, BLUE);
    TEST_ASSERT_EQUAL_HEX32(BLUE, phys.getPixelColor(8));
    TEST_ASSERT_EQUAL_HEX32(BLUE, phys.getPixelColor(11));

    layout.fill(GREEN);
    TEST_ASSERT_EQUAL_HEX32(GREEN, phys.getPixelColor(0));
    TEST_ASSERT_EQUAL_HEX32(GREEN, phys.getPixelColor(19));
    TEST_ASSERT_EQUAL_HEX32(BLACK, phys.getPixelColor(9));
    TEST_ASSERT_EQUAL_HEX32(BLACK, phys.getPixelColor(10));
}

// Negative dead LEDs on a mirrored strip sit at both physical edges and shift
// the mirrored mapping inward.
void test_mirror_negative_dead_clears_edges() {
    MockStrip phys(20);
    phys.fill(WHITE);
    Strip::Layout layout(phys, false, true, -2);

    TEST_ASSERT_EQUAL_INT16(9, layout.length());
    TEST_ASSERT_EQUAL_HEX32(BLACK, phys.getPixelColor(0));
    TEST_ASSERT_EQUAL_HEX32(BLACK, phys.getPixelColor(19));
    TEST_ASSERT_EQUAL_HEX32(WHITE, phys.getPixelColor(1));
    TEST_ASSERT_EQUAL_HEX32(WHITE, phys.getPixelColor(18));

    layout.setPixelColor(0, RED);
    TEST_ASSERT_EQUAL_HEX32(RED, phys.getPixelColor(1));
    TEST_ASSERT_EQUAL_HEX32(RED, phys.getPixelColor(18));
    TEST_ASSERT_EQUAL_HEX32(RED, layout.getPixelColor(0));
}

// reverse is applied in logical space before the mirror pair is resolved.
void test_reverse_and_mirror_combined() {
    MockStrip phys(20);
    Strip::Layout layout(phys, true, true, 0);

    TEST_ASSERT_EQUAL_INT16(10, layout.length());

    layout.setPixelColor(0, RED);
    TEST_ASSERT_EQUAL_HEX32(RED, phys.getPixelColor(9));
    TEST_ASSERT_EQUAL_HEX32(RED, phys.getPixelColor(10));
    TEST_ASSERT_EQUAL_HEX32(RED, layout.getPixelColor(0));

    layout.setPixelColor(9, BLUE);
    TEST_ASSERT_EQUAL_HEX32(BLUE, phys.getPixelColor(0));
    TEST_ASSERT_EQUAL_HEX32(BLUE, phys.getPixelColor(19));
}

// reverse + positive plain dead: reversal happens first, then the offset.
void test_reverse_plain_positive_dead() {
    MockStrip phys(10);
    phys.fill(WHITE);
    Strip::Layout layout(phys, true, false, 2);

    TEST_ASSERT_EQUAL_INT16(8, layout.length());
    TEST_ASSERT_EQUAL_HEX32(BLACK, phys.getPixelColor(0));
    TEST_ASSERT_EQUAL_HEX32(BLACK, phys.getPixelColor(1));

    layout.setPixelColor(0, RED);
    TEST_ASSERT_EQUAL_HEX32(RED, phys.getPixelColor(9));
    layout.setPixelColor(7, BLUE);
    TEST_ASSERT_EQUAL_HEX32(BLUE, phys.getPixelColor(2));
    TEST_ASSERT_EQUAL_HEX32(RED, layout.getPixelColor(0));
}

// show() and brightness are delegated straight through to the wrapped strip.
void test_show_and_brightness_delegate() {
    MockStrip phys(4);
    Strip::Layout layout(phys, false, false, 0);

    layout.show();
    layout.setBrightness(128);
    TEST_ASSERT_EQUAL_UINT8(255, layout.getBrightness());
}

int runUnityTests() {
    UNITY_BEGIN();
    RUN_TEST(test_plain_no_dead_is_identity);
    RUN_TEST(test_reverse_maps_index_from_far_end);
    RUN_TEST(test_plain_positive_dead_clears_beginning);
    RUN_TEST(test_plain_negative_dead_clears_end);
    RUN_TEST(test_mirror_positive_dead_clears_middle);
    RUN_TEST(test_mirror_negative_dead_clears_edges);
    RUN_TEST(test_reverse_and_mirror_combined);
    RUN_TEST(test_reverse_plain_positive_dead);
    RUN_TEST(test_show_and_brightness_delegate);
    return UNITY_END();
}

int main() {
    return runUnityTests();
}