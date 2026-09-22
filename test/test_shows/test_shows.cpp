#include "unity.h"
#include "../MockStrip.h"
#include "show/Fire.h"
#include "show/Rainbow.h"
#include "show/Wave.h"

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

    auto show = new Show::Fire(cooling, spread, ignition, 0.5f, {1.0f}, 5, 5);

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
    auto show = new Show::Rainbow(1.0f, 1.0f);
    MockStrip strip(10);
    show->execute(strip, 0);
    TEST_ASSERT_NOT_NULL(show);
    delete show;
}

void test_rainbow_default_execute_runs_without_crash() {
    Show::Rainbow show(1.0f, 1.0f);
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

// Wave show tests
void test_wave_default_constructor_runs() {
    auto show = new Show::Wave(2.0f, 0.1f, Show::WaveMode::Bounce);
    TEST_ASSERT_NOT_NULL(show);
    delete show;
}

void test_wave_default_execute_runs_without_crash() {
    Show::Wave show(2.0f, 0.1f, Show::WaveMode::Bounce);
    MockStrip strip(30);
    for (Show::Iteration t = 0; t < 5; t++) {
        show.execute(strip, t);
    }
    TEST_ASSERT_EQUAL_UINT32(30, strip.length());
}

void test_wave_explicit_constructor_does_not_crash() {
    auto show = new Show::Wave(3.5f, 0.5f, Show::WaveMode::Bounce);
    MockStrip strip(60);
    show->execute(strip, 0);
    show->execute(strip, 42);
    delete show;
    TEST_PASS();
}

void test_wave_symmetric_lighting_around_mid_source() {
    // Brightness frequency 0.5 cycles/sec, source reaches the midpoint of its
    // first half-bounce when t * 0.5 * 2π = π/2, i.e. time = 0.5s. Each
    // execute() call advances time by 0.05, so call execute() ten times to
    // land the source at pixel 9-10 on a 20-pixel strip.
    Show::Wave show(1.0f, 0.5f, Show::WaveMode::Bounce);
    MockStrip strip(20);
    for (Show::Iteration t = 0; t < 10; t++) {
        show.execute(strip, t);
    }

    // The source is mid-strip, so pixels on either side near the centre must
    // both be lit (non-black). The envelope decays symmetrically from the
    // source, so a few pixels either side must also be lit.
    TEST_ASSERT_TRUE_MESSAGE(strip.getPixelColor(9) != 0, "left of source is black");
    TEST_ASSERT_TRUE_MESSAGE(strip.getPixelColor(10) != 0, "right of source is black");

    for (Strip::PixelIndex i = 7; i <= 12; i++) {
        TEST_ASSERT_TRUE_MESSAGE(strip.getPixelColor(i) != 0, "expected lit pixel near source");
    }
}

void test_wave_bounce_and_traveling_modes_are_identical() {
    // Both modes are accepted by the JSON contract but currently produce
    // identical output (the wavelength-based stripe layer they used to
    // differentiate was removed). Two Wave shows with the same decay_rate
    // and brightness_frequency but different mode values must produce
    // pixel-identical strips when executed for the same number of
    // iterations.
    Show::Wave bounce(2.0f, 0.1f, Show::WaveMode::Bounce);
    Show::Wave traveling(2.0f, 0.1f, Show::WaveMode::Traveling);
    MockStrip strip_b(60);
    MockStrip strip_t(60);
    for (Show::Iteration it = 0; it < 50; it++) {
        bounce.execute(strip_b, it);
        traveling.execute(strip_t, it);
    }

    for (Strip::PixelIndex i = 0; i < strip_b.length(); i++) {
        TEST_ASSERT_EQUAL_HEX32_MESSAGE(strip_t.getPixelColor(i),
                                        strip_b.getPixelColor(i),
                                        "bounce and traveling modes must produce identical pixels");
    }
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

    // Wave show
    RUN_TEST(test_wave_default_constructor_runs);
    RUN_TEST(test_wave_default_execute_runs_without_crash);
    RUN_TEST(test_wave_explicit_constructor_does_not_crash);
    RUN_TEST(test_wave_symmetric_lighting_around_mid_source);
    RUN_TEST(test_wave_bounce_and_traveling_modes_are_identical);

    return UNITY_END();
}

int main() {
    return runUnityTests();
}
