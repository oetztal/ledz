#include "unity.h"
#include "../MockStrip.h"
#include "show/Fire.h"
#include "show/Rainbow.h"

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

    // With double-buffering, heat only spreads one step per frame
    // Heat from index 0 spreads to index 1, not teleporting to index 9
    TEST_ASSERT_EQUAL_FLOAT(0.75f, state->get_temperature(0));
    TEST_ASSERT_EQUAL_FLOAT(0.25f, state->get_temperature(1));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, state->get_temperature(9));
}

void test_spread_limited() {
    state->set_temperature(0, 0.1f);
    state->spread(1.0, 0.0, 0, 0.5f);

    // With double-buffering, heat spreads to adjacent pixel only
    TEST_ASSERT_EQUAL_FLOAT(0.0f, state->get_temperature(0));
    TEST_ASSERT_EQUAL_FLOAT(0.1f, state->get_temperature(1));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, state->get_temperature(9));
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

    // With double-buffering, all reads come from the snapshot.
    // weights = {2.0f, 1.0f} -> at i=2, weights for prev_idx 1 and 0
    // i=1: takes 0.25 from index 0 (temp[0]: 1.0->0.75, temp[1]: 1.0->1.25)
    // i=2: reads prev_temp[1]=1.0, prev_temp[0]=1.0, spreads 0.25
    //      temp[1] -= 0.25*2/3, temp[0] -= 0.25*1/3
    // i=3: reads prev_temp[2]=0, prev_temp[1]=1.0, spreads 0.25
    //      temp[2] -= 0.25*2/3, temp[1] -= 0.25*1/3

    state->spread(1.0, 0.0, 0, 0.5f, {2.0f, 1.0f});

    // Verify energy conservation
    float total = 0;
    for (int i = 0; i < state->length(); i++) {
        total += state->get_temperature(i);
    }
    TEST_ASSERT_EQUAL_FLOAT(2.0f, total);

    TEST_ASSERT_EQUAL_FLOAT(0.6666667f, state->get_temperature(0));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, state->get_temperature(1));
    TEST_ASSERT_EQUAL_FLOAT(0.0833333f, state->get_temperature(2));
}

void test_spark_amount() {
    state->set_temperature(0, 0.0f);
    state->spread(0.0, 1.0, 1, 0.7f);
    TEST_ASSERT_EQUAL_FLOAT(0.7f, state->get_temperature(0));
}

// Rainbow show tests
void test_rainbow_default_constructor_runs() {
    auto show = new Show::Rainbow();
    MockStrip strip(10);
    show->execute(strip, 0);
    TEST_ASSERT_NOT_NULL(show);
    delete show;
}

void test_rainbow_default_execute_runs_without_crash() {
    Show::Rainbow show;
    MockStrip strip(30);
    for (Show::Iteration t = 0; t < 5; t++) {
        show.execute(strip, t);
    }
    TEST_ASSERT_EQUAL_UINT32(30, strip.length());
}

void test_rainbow_pixel_step_zero_all_pixels_share_hue() {
    Show::Rainbow show(1.0f, 0.0f);
    MockStrip strip(10);
    show.execute(strip, 0);
    auto first = strip.getPixelColor(0);
    for (int i = 1; i < 10; i++) {
        TEST_ASSERT_EQUAL_HEX32(first, strip.getPixelColor(i));
    }
}

void test_rainbow_time_step_zero_hue_advances_with_pixel() {
    Show::Rainbow show(0.0f, 1.0f);
    MockStrip strip(20);
    show.execute(strip, 0);
    TEST_ASSERT_EQUAL_HEX32(strip.getPixelColor(0), strip.getPixelColor(0));
    bool saw_difference = false;
    for (int i = 1; i < 20; i++) {
        if (strip.getPixelColor(i) != strip.getPixelColor(0)) {
            saw_difference = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(saw_difference);
}

void test_rainbow_explicit_constructor_does_not_crash() {
    auto show = new Show::Rainbow(2.5f, 0.5f);
    MockStrip strip(15);
    show->execute(strip, 100);
    delete show;
    TEST_PASS();
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

    // Rainbow show
    RUN_TEST(test_rainbow_default_constructor_runs);
    RUN_TEST(test_rainbow_default_execute_runs_without_crash);
    RUN_TEST(test_rainbow_pixel_step_zero_all_pixels_share_hue);
    RUN_TEST(test_rainbow_time_step_zero_hue_advances_with_pixel);
    RUN_TEST(test_rainbow_explicit_constructor_does_not_crash);

    return UNITY_END();
}

int main() {
    return runUnityTests();
}
