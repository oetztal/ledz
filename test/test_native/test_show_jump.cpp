#include "unity.h"
#include "show/Jump.h"

Show::Jump::Ball ball(1.0f, 0xff0000);

void setUp() {
}

void tearDown() {
    // clean stuff up here
}

void test_function_should_doBlahAndBlah() {
    auto position = ball.get_position(0, 100);
    TEST_ASSERT_EQUAL_INT(0, position);
}

void test_start_of_peak() {
    auto position = ball.get_position(90, 100);
    TEST_ASSERT_EQUAL_INT(99, position);
}

void test_mid_of_peak() {
    auto position = ball.get_position(100, 100);
    TEST_ASSERT_EQUAL_INT(100, position);
}

void test_end_of_peak() {
    auto position = ball.get_position(110, 100);
    TEST_ASSERT_EQUAL_INT(99, position);
}

int runUnityTests() {
    UNITY_BEGIN();
    RUN_TEST(test_function_should_doBlahAndBlah);
    RUN_TEST(test_start_of_peak);
    RUN_TEST(test_mid_of_peak);
    RUN_TEST(test_end_of_peak);
    return UNITY_END();
}

int main() {
    return runUnityTests();
}
