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

// Wave show tests
void test_wave_default_constructor_runs() {
    auto show = new Show::Wave();
    TEST_ASSERT_NOT_NULL(show);
    delete show;
}

void test_wave_default_execute_runs_without_crash() {
    Show::Wave show;
    MockStrip strip(30);
    for (Show::Iteration t = 0; t < 5; t++) {
        show.execute(strip, t);
    }
    TEST_ASSERT_EQUAL_UINT32(30, strip.length());
}

void test_wave_explicit_constructor_does_not_crash() {
    auto show = new Show::Wave(3.5f, 0.5f, 10.0f);
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
    Show::Wave show(1.0f, 0.5f, 6.0f);
    MockStrip strip(20);
    for (Show::Iteration t = 0; t < 10; t++) {
        show.execute(strip, t);
    }

    // The source is mid-strip, so pixels on either side near the centre must
    // both be lit (non-black).
    TEST_ASSERT_TRUE_MESSAGE(strip.getPixelColor(9) != 0, "left of source is black");
    TEST_ASSERT_TRUE_MESSAGE(strip.getPixelColor(10) != 0, "right of source is black");

    // Every pixel's brightness contribution from the wave must be non-negative:
    // since the colour is wheel(emission_time) * brightness, and brightness is
    // the product of non-negative factors, the result has each channel scaled
    // by a non-negative factor. Verify a few pixels either side are non-black.
    for (Strip::PixelIndex i = 7; i <= 12; i++) {
        TEST_ASSERT_TRUE_MESSAGE(strip.getPixelColor(i) != 0, "expected lit pixel near source");
    }
}

void test_wave_traveling_mode_stripes_drift_independent_of_source() {
    // Wave traveling mode: phase = 2π * (i/λ - t * freq). Stripes drift at
    // freq * λ pixels per second independent of source motion.
    //
    // We use decay_rate = 0 so the envelope is uniformly 1 across the strip;
    // otherwise the bouncing source position would modulate brightness
    // independently of the wave and obscure the drift. The drift rate
    // λ * freq is what we are testing; the envelope is a multiplicative
    // factor that is identical in both modes.
    constexpr float freq = 0.1f;
    constexpr float wavelength = 6.0f;
    constexpr Show::Iteration drift_iters = 50;  // t = 2.5 s, drift = 1.5 px

    auto totalBrightness = [](Strip::Color c) {
        return static_cast<int>(red(c)) + green(c) + blue(c);
    };

    // Sample the brightness profile at t = 0 on a fresh wave.
    Show::Wave show_first(0.0f, freq, wavelength, Show::WaveMode::Traveling);
    MockStrip strip_first(30);
    show_first.execute(strip_first, 0);

    // Sample at t = 2.5 s on another fresh wave.
    Show::Wave show_later(0.0f, freq, wavelength, Show::WaveMode::Traveling);
    MockStrip strip_later(30);
    for (Show::Iteration t = 0; t <= drift_iters; t++) {
        show_later.execute(strip_later, t);
    }

    // Compare the brightness profiles via integer-pixel cross-correlation.
    // The shift that maximises the inner product is the wave drift. The
    // expected drift is 1.5 px; with discrete integer shifts the peak
    // should land at 1 or 2.
    long best_correlation = -1;
    int best_shift = 0;
    for (int s = -3; s <= 3; s++) {
        long corr = 0;
        for (Strip::PixelIndex i = 5; i < 25; i++) {
            int j = static_cast<int>(i) + s;
            if (j < 0 || j >= static_cast<int>(strip_first.length())) continue;
            corr += static_cast<long>(totalBrightness(strip_first.getPixelColor(i)))
                  * static_cast<long>(totalBrightness(strip_later.getPixelColor(
                        static_cast<Strip::PixelIndex>(j))));
        }
        if (corr > best_correlation) {
            best_correlation = corr;
            best_shift = s;
        }
    }

    TEST_ASSERT_TRUE_MESSAGE(best_shift == 1 || best_shift == 2,
                             "traveling-mode stripes should have drifted by ~1.5 px");
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
    RUN_TEST(test_wave_traveling_mode_stripes_drift_independent_of_source);

    return UNITY_END();
}

int main() {
    return runUnityTests();
}
