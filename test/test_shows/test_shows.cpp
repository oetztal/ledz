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
    TEST_ASSERT_EQUAL(0.0f, state->get_temperature(0));
}

void test_cooldown_limited_at_zero() {
    state->cooldown(1.0);

    TEST_ASSERT_EQUAL(0.0f, state->get_temperature(0));
}

void test_cooldown() {
    state->set_temperature(0, 1.5f);

    state->cooldown(1.0);

    TEST_ASSERT_EQUAL(0.5f, state->get_temperature(0));
}

void test_spread() {
    state->set_temperature(0, 1.0f);
    state->spread(1.0, 0.0, 0, 0.5f);

    TEST_ASSERT_EQUAL_FLOAT(0.75f, state->get_temperature(0));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, state->get_temperature(1));
    TEST_ASSERT_EQUAL_FLOAT(0.25f, state->get_temperature(9));
}

void test_spread_limited() {
    state->set_temperature(0, 0.1f);
    state->spread(1.0, 0.0, 0, 0.5f);

    TEST_ASSERT_EQUAL_FLOAT(0.0f, state->get_temperature(0));
    TEST_ASSERT_EQUAL_FLOAT(0.1f, state->get_temperature(9));
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
    state->set_temperature(0, 1.0f);
    state->set_temperature(1, 1.0f);

    // Element 2 will take from 1 and 0.
    // weights = {2.0f, 1.0f} -> total_weight = 3.0f
    // w0 = 2/3, w1 = 1/3
    // weighted_previous for i=2: get_temperature(1]*2/3 + get_temperature(0]*1/3 = 1.0*2/3 + 1.0*1/3 = 1.0
    // spreadValue = min(0.25, 1.0) * 1.0 * 1.0 = 0.25
    // spread = min(1.0, 0.25) = 0.25
    // get_temperature(2] += 0.25 -> 0.25
    // get_temperature(1] -= 0.25 * (2/3) = 0.25 * 0.666... = 0.1666... -> 1.0 - 0.1666... = 0.8333...
    // get_temperature(0] -= 0.25 * (1/3) = 0.25 * 0.333... = 0.0833... -> 1.0 - 0.0833... = 0.9166...

    state->spread(1.0, 0.0, 0, 0.5f, {2.0f, 1.0f});

    float total = 0;
    for (int i = 0; i < state->length(); i++) {
        total += state->get_temperature(i);
    }
    TEST_ASSERT_EQUAL_FLOAT(2.0f, total);

    TEST_ASSERT_EQUAL_FLOAT(0.6666667f, state->get_temperature(0));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, state->get_temperature(1));
    TEST_ASSERT_EQUAL_FLOAT(0.01851851f, state->get_temperature(2));
}

void test_spark_amount() {
    state->set_temperature(0, 0.0f);
    state->spread(0.0, 1.0, 1, 0.7f);
    TEST_ASSERT_EQUAL_FLOAT(0.7f, state->get_temperature(0));
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
    RUN_TEST(test_spark_amount);
    RUN_TEST(test_create_fire);

    return UNITY_END();
}

int main() {
    return runUnityTests();
}
