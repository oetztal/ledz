#include "unity.h"

#include "Config.h"

// A default TimerEntry carries no brightness: -1 is the "not set" sentinel,
// which is also the default read for a slot stored by older firmware.
void test_default_timer_entry_has_no_brightness() {
    const Config::TimerEntry timer;
    TEST_ASSERT_EQUAL_INT16(-1, timer.brightness);
}

// 0 is a valid brightness, distinct from "not set" (design decision 1).
void test_zero_brightness_is_distinct_from_unset() {
    Config::TimerEntry timer;
    TEST_ASSERT_TRUE(timer.brightness < 0); // unset

    timer.brightness = 0;
    TEST_ASSERT_EQUAL_INT16(0, timer.brightness);
    TEST_ASSERT_TRUE(timer.brightness >= 0); // set
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_default_timer_entry_has_no_brightness);
    RUN_TEST(test_zero_brightness_is_distinct_from_unset);
    return UNITY_END();
}
