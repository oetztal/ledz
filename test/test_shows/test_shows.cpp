#include "unity.h"
#include "../MockStrip.h"
#include "show/Fire.h"

Show::FireState *state;

void setUp() {
    state = new Show::FireState([] { return 1.0f; }, 10);
}

void tearDown() {
    delete state;
}

void test_default_value_zero() {
    TEST_ASSERT_EQUAL(0.0f, state->temperature[0]);
}

void test_cooldown_limited_at_zero() {
    state->cooldown(1.0);

    TEST_ASSERT_EQUAL(0.0f, state->temperature[0]);
}

void test_cooldown() {
    state->temperature[0] = 1.5f;

    state->cooldown(1.0);

    TEST_ASSERT_EQUAL(0.5f, state->temperature[0]);
}

void test_spread() {
    state->spread(1.0, 1.0, 5);

    TEST_ASSERT_EQUAL_FLOAT(1.75f, state->temperature[0]);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, state->temperature[1]);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, state->temperature[2]);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, state->temperature[3]);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, state->temperature[4]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, state->temperature[5]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, state->temperature[6]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, state->temperature[7]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, state->temperature[8]);
    TEST_ASSERT_EQUAL_FLOAT(0.25f, state->temperature[9]);
}

void test_spread_limited() {
    delete state;
    std::vector<float> results{
        0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f
    };
    state = new Show::FireState([&] {
        auto value = results.back();
        results.pop_back();
        return value;
    }, 10);

    state->spread(1.0, 1.0, 5);

    TEST_ASSERT_EQUAL_FLOAT(2.0f, state->temperature[0]);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, state->temperature[1]);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, state->temperature[2]);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, state->temperature[3]);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, state->temperature[4]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, state->temperature[5]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, state->temperature[6]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, state->temperature[7]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, state->temperature[8]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, state->temperature[9]);
}

void test_create_fire() {
    auto cooling = 1.0;
    auto spread = 1.0;
    auto ignition = 1.0;

    auto show = new Show::Fire(cooling, spread, ignition);

    TEST_ASSERT_NOT_NULL(show);
}

int runUnityTests() {
    UNITY_BEGIN();

    // Test parseColors logic (used by ShowFactory)
    RUN_TEST(test_default_value_zero);
    RUN_TEST(test_cooldown_limited_at_zero);
    RUN_TEST(test_cooldown);
    RUN_TEST(test_spread);
    RUN_TEST(test_spread_limited);
    RUN_TEST(test_create_fire);

    return UNITY_END();
}

int main() {
    return runUnityTests();
}
