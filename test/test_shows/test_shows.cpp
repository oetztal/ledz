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

void test_spread_multiple_weights() {
    delete state;
    state = new Show::FireState([] { return 1.0f; }, 10);
    state->temperature[0] = 1.0f;
    state->temperature[1] = 1.0f;

    // Element 2 will take from 1 and 0.
    // weights = {2.0f, 1.0f} -> total_weight = 3.0f
    // w0 = 2/3, w1 = 1/3
    // weighted_previous for i=2: temperature[1]*2/3 + temperature[0]*1/3 = 1.0*2/3 + 1.0*1/3 = 1.0
    // spreadValue = min(0.25, 1.0) * 1.0 * 1.0 = 0.25
    // spread = min(1.0, 0.25) = 0.25
    // temperature[2] += 0.25 -> 0.25
    // temperature[1] -= 0.25 * (2/3) = 0.25 * 0.666... = 0.1666... -> 1.0 - 0.1666... = 0.8333...
    // temperature[0] -= 0.25 * (1/3) = 0.25 * 0.333... = 0.0833... -> 1.0 - 0.0833... = 0.9166...

    state->spread(1.0, 0.0, 0, {2.0f, 1.0f});

    float total = 0;
    for (int i = 0; i < state->length(); i++) {
        total += state->temperature[i];
    }
    TEST_ASSERT_EQUAL_FLOAT(2.0f, total);

    TEST_ASSERT_EQUAL_FLOAT(0.6666667f, state->temperature[0]);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, state->temperature[1]);
    TEST_ASSERT_EQUAL_FLOAT(0.01851851f, state->temperature[2]);
}

int runUnityTests() {
    UNITY_BEGIN();

    // Test parseColors logic (used by ShowFactory)
    RUN_TEST(test_default_value_zero);
    RUN_TEST(test_cooldown_limited_at_zero);
    RUN_TEST(test_cooldown);
    RUN_TEST(test_spread);
    RUN_TEST(test_spread_limited);
    RUN_TEST(test_spread_multiple_weights);
    RUN_TEST(test_create_fire);

    return UNITY_END();
}

int main() {
    return runUnityTests();
}
