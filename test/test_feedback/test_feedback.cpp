#include <cmath>

#include "unity.h"
#include "../MockStrip.h"
#include "support/Color.h"
#include "support/Feedback.h"

using Support::Feedback;
using Support::FeedbackTransform;

static const Strip::Color WHITE = 0xFFFFFF;
static const Strip::Color RED = 0xFF0000;

void setUp() {}

void tearDown() {}

// Run one feedback frame without emission and render it.
static void step(Feedback& fb, MockStrip& strip, const FeedbackTransform& tf, float decay, uint64_t t) {
    fb.begin(strip.length(), tf, decay, t);
    fb.render(strip);
}

static bool allBlack(const MockStrip& strip) {
    for (Strip::PixelIndex i = 0; i < strip.length(); i++) {
        if (strip.getPixelColor(i) != 0) return false;
    }
    return true;
}

// --- persistence ------------------------------------------------------------

void test_state_starts_black() {
    Feedback fb;
    MockStrip strip(20);
    strip.fill(WHITE); // render must overwrite whatever the strip held
    step(fb, strip, {}, 0.9f, 0);
    TEST_ASSERT_TRUE(allBlack(strip));
}

void test_emitted_light_persists_after_emission_stops() {
    Feedback fb;
    MockStrip strip(10);
    fb.begin(10, {}, 0.9f, 0);
    fb.addPixel(4, RED);
    fb.render(strip);
    TEST_ASSERT_EQUAL_HEX32(RED, strip.getPixelColor(4));

    step(fb, strip, {}, 0.9f, 1);
    TEST_ASSERT_EQUAL_UINT8(230, Support::Color::red(strip.getPixelColor(4)));
    TEST_ASSERT_EQUAL_UINT8(0, Support::Color::green(strip.getPixelColor(4)));
    TEST_ASSERT_EQUAL_UINT8(0, Support::Color::blue(strip.getPixelColor(4)));
}

// --- decay ------------------------------------------------------------------

void test_geometric_decay() {
    const float d = 0.8f;
    Feedback fb;
    fb.begin(10, {}, d, 0);
    fb.addPixel(3, WHITE);
    for (int k = 1; k <= 10; k++) {
        fb.begin(10, {}, d, k);
        for (int ch = 0; ch < 3; ch++) {
            TEST_ASSERT_FLOAT_WITHIN(1e-5f, std::pow(d, static_cast<float>(k)), fb.intensity(3, ch));
        }
    }
}

void test_decay_one_keeps_previous_frame() {
    Feedback fb;
    fb.begin(10, {}, 1.0f, 0);
    fb.addPixel(3, WHITE, 0.4f);
    for (int k = 1; k <= 20; k++) {
        fb.begin(10, {}, 1.0f, k);
    }
    TEST_ASSERT_EQUAL_FLOAT(0.4f, fb.intensity(3, 0));
}

void test_zero_decay_leaves_only_fresh_emission() {
    Feedback fb;
    MockStrip strip(10);
    fb.begin(10, {}, 0.0f, 0);
    fb.addPixel(2, WHITE);
    fb.render(strip);

    fb.begin(10, {}, 0.0f, 1);
    fb.addPixel(7, RED);
    fb.render(strip);
    for (Strip::PixelIndex i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL_HEX32(i == 7 ? RED : 0, strip.getPixelColor(i));
    }
}

void test_trail_fades_to_true_black_within_60_iterations() {
    Feedback fb;
    MockStrip strip(10);
    fb.begin(10, {}, 0.9f, 0);
    fb.addPixel(5, WHITE);
    fb.render(strip);

    int black_at = -1;
    for (int k = 1; k <= 100; k++) {
        step(fb, strip, {}, 0.9f, k);
        if (strip.getPixelColor(5) == 0) {
            if (black_at < 0) black_at = k;
        } else {
            TEST_ASSERT_LESS_THAN_MESSAGE(0, black_at, "pixel lit again after reaching black");
        }
    }
    TEST_ASSERT_GREATER_THAN(0, black_at);
    TEST_ASSERT_LESS_OR_EQUAL(60, black_at);
}

// --- emission ---------------------------------------------------------------

void test_overlapping_emission_saturates() {
    Feedback fb;
    MockStrip strip(10);
    fb.begin(10, {}, 1.0f, 0);
    fb.addPixel(1, RED, 0.7f);
    fb.addPixel(1, RED, 0.7f);
    fb.render(strip);
    TEST_ASSERT_EQUAL_HEX32(RED, strip.getPixelColor(1));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, fb.intensity(1, 0));
}

void test_add_splits_between_neighbours() {
    Feedback fb;
    fb.begin(10, {}, 1.0f, 0);
    fb.add(3.25f, WHITE);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.75f, fb.intensity(3, 0));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.25f, fb.intensity(4, 0));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, fb.intensity(2, 0));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, fb.intensity(5, 0));
}

void test_add_out_of_range_is_ignored() {
    Feedback fb;
    MockStrip strip(10);
    fb.begin(10, {}, 1.0f, 0);
    fb.add(-5.0f, WHITE);
    fb.add(42.0f, WHITE);
    fb.addPixel(-1, WHITE);
    fb.addPixel(10, WHITE);
    fb.render(strip);
    TEST_ASSERT_TRUE(allBlack(strip));
}

// --- zoom -------------------------------------------------------------------

static Strip::PixelIndex brightest(const Feedback& fb, Strip::PixelIndex n) {
    Strip::PixelIndex best = 0;
    for (Strip::PixelIndex i = 1; i < n; i++) {
        if (fb.intensity(i, 0) > fb.intensity(best, 0)) best = i;
    }
    return best;
}

void test_zoom_below_one_moves_content_outward() {
    const Strip::PixelIndex n = 60;
    const float c = (n - 1) * 0.5f;
    FeedbackTransform tf;
    tf.zoom = 0.9f;

    // One lit pixel on each side of the centre, to cover both directions.
    for (Strip::PixelIndex start : {40, 19}) {
        Feedback fb;
        fb.begin(n, tf, 1.0f, 0);
        fb.addPixel(start, WHITE);
        float distance = std::fabs(start - c);
        for (int k = 1; k <= 5; k++) {
            fb.begin(n, tf, 1.0f, k);
            float next = std::fabs(brightest(fb, n) - c);
            TEST_ASSERT_GREATER_THAN_FLOAT(distance, next);
            distance = next;
        }
    }
}

void test_zoom_above_one_moves_content_inward() {
    const Strip::PixelIndex n = 60;
    const float c = (n - 1) * 0.5f;
    FeedbackTransform tf;
    tf.zoom = 1.2f;
    Feedback fb;
    fb.begin(n, tf, 1.0f, 0);
    fb.addPixel(55, WHITE);
    fb.begin(n, tf, 1.0f, 1);
    TEST_ASSERT_LESS_THAN_FLOAT(55 - c, std::fabs(brightest(fb, n) - c));
}

void test_zoom_one_is_identity() {
    const Strip::PixelIndex n = 31;
    Feedback fb;
    MockStrip before(n);
    MockStrip after(n);
    fb.begin(n, {}, 1.0f, 0);
    for (Strip::PixelIndex i = 0; i < n; i++) {
        fb.addPixel(i, Support::Color::wheel(static_cast<float>(i) * 8.0f), static_cast<float>(i) / n);
    }
    fb.render(before);
    step(fb, after, {}, 1.0f, 1);
    for (Strip::PixelIndex i = 0; i < n; i++) {
        TEST_ASSERT_EQUAL_HEX32(before.getPixelColor(i), after.getPixelColor(i));
    }
}

// --- scroll -----------------------------------------------------------------

void test_scroll_wraps_around_the_end() {
    const Strip::PixelIndex n = 10;
    FeedbackTransform tf;
    tf.scroll = 1.0f;
    Feedback fb;
    MockStrip strip(n);
    fb.begin(n, tf, 1.0f, 0);
    fb.addPixel(n - 1, WHITE);
    step(fb, strip, tf, 1.0f, 1);
    TEST_ASSERT_EQUAL_HEX32(WHITE, strip.getPixelColor(0));
    TEST_ASSERT_EQUAL_HEX32(0, strip.getPixelColor(n - 1));
}

void test_negative_scroll_wraps_around_the_start() {
    const Strip::PixelIndex n = 10;
    FeedbackTransform tf;
    tf.scroll = -1.0f;
    Feedback fb;
    MockStrip strip(n);
    fb.begin(n, tf, 1.0f, 0);
    fb.addPixel(0, WHITE);
    step(fb, strip, tf, 1.0f, 1);
    TEST_ASSERT_EQUAL_HEX32(WHITE, strip.getPixelColor(n - 1));
    TEST_ASSERT_EQUAL_HEX32(0, strip.getPixelColor(0));
}

// --- warp -------------------------------------------------------------------

void test_zero_warp_changes_nothing() {
    const Strip::PixelIndex n = 40;
    FeedbackTransform plain;
    plain.zoom = 0.95f;
    FeedbackTransform zero_warp = plain;
    zero_warp.warp_amp = 0.0f;
    zero_warp.warp_freq = 1.7f;
    zero_warp.warp_speed = 0.9f;

    Feedback a;
    Feedback b;
    MockStrip sa(n);
    MockStrip sb(n);
    for (uint64_t t = 0; t < 30; t++) {
        a.begin(n, plain, 0.97f, t);
        b.begin(n, zero_warp, 0.97f, t);
        a.add(19.5f, Support::Color::wheel(static_cast<float>(t) * 3.0f));
        b.add(19.5f, Support::Color::wheel(static_cast<float>(t) * 3.0f));
        a.render(sa);
        b.render(sb);
        for (Strip::PixelIndex i = 0; i < n; i++) {
            TEST_ASSERT_EQUAL_HEX32(sa.getPixelColor(i), sb.getPixelColor(i));
        }
    }
}

void test_warp_moves_content() {
    const Strip::PixelIndex n = 40;
    FeedbackTransform tf;
    tf.warp_amp = 2.0f;
    Feedback fb;
    fb.begin(n, tf, 1.0f, 0);
    fb.addPixel(20, WHITE);
    fb.begin(n, tf, 1.0f, 1);
    TEST_ASSERT_TRUE(fb.intensity(20, 0) < 1.0f);
}

// --- push -------------------------------------------------------------------

void test_push_moves_content_outward_on_both_sides() {
    const Strip::PixelIndex n = 21; // odd, so the centre c = 10 is a pixel
    FeedbackTransform tf;
    tf.push = 1.0f;
    for (Strip::PixelIndex start : {15, 5}) {
        const Strip::PixelIndex expected = start > 10 ? start + 1 : start - 1;
        Feedback fb;
        MockStrip strip(n);
        fb.begin(n, tf, 1.0f, 0);
        fb.addPixel(start, WHITE);
        step(fb, strip, tf, 1.0f, 1);
        for (Strip::PixelIndex i = 0; i < n; i++) {
            TEST_ASSERT_EQUAL_HEX32(i == expected ? WHITE : 0, strip.getPixelColor(i));
        }
    }
}

void test_push_clears_the_centre_pixel() {
    FeedbackTransform tf;
    tf.push = 1.0f;
    Feedback fb;
    fb.begin(21, tf, 1.0f, 0);
    fb.addPixel(10, WHITE);
    fb.begin(21, tf, 1.0f, 1);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, fb.intensity(10, 0));
}

void test_push_clears_the_centre_pair_on_even_lengths() {
    FeedbackTransform tf;
    tf.push = 1.0f;
    Feedback fb;
    fb.begin(20, tf, 1.0f, 0);
    fb.addPixel(9, WHITE);
    fb.addPixel(10, WHITE);
    fb.begin(20, tf, 1.0f, 1);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, fb.intensity(9, 0));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, fb.intensity(10, 0));
}

void test_zero_push_changes_nothing() {
    const Strip::PixelIndex n = 40;
    FeedbackTransform plain;
    plain.zoom = 0.93f;
    plain.warp_amp = 1.0f;
    FeedbackTransform zero_push = plain;
    zero_push.push = 0.0f;

    Feedback a;
    Feedback b;
    MockStrip sa(n);
    MockStrip sb(n);
    for (uint64_t t = 0; t < 30; t++) {
        a.begin(n, plain, 0.97f, t);
        b.begin(n, zero_push, 0.97f, t);
        a.add(19.5f, Support::Color::wheel(static_cast<float>(t) * 3.0f));
        b.add(19.5f, Support::Color::wheel(static_cast<float>(t) * 3.0f));
        a.render(sa);
        b.render(sb);
        for (Strip::PixelIndex i = 0; i < n; i++) {
            TEST_ASSERT_EQUAL_HEX32(sa.getPixelColor(i), sb.getPixelColor(i));
        }
    }
}

// --- mirror -----------------------------------------------------------------

void test_mirror_makes_output_symmetric() {
    for (Strip::PixelIndex n : {20, 21}) {
        FeedbackTransform tf;
        tf.mirror = true;
        tf.zoom = 0.93f;
        tf.warp_amp = 1.5f;
        tf.push = 0.7f;
        Feedback fb;
        MockStrip strip(n);
        for (uint64_t t = 0; t < 10; t++) {
            fb.begin(n, tf, 0.95f, t);
            // Asymmetric emission: the next remap must still fold it symmetric.
            fb.add(2.3f + static_cast<float>(t), Support::Color::wheel(static_cast<float>(t) * 20.0f));
        }
        step(fb, strip, tf, 0.95f, 10);
        for (Strip::PixelIndex i = 0; i < n; i++) {
            TEST_ASSERT_EQUAL_HEX32(strip.getPixelColor(i), strip.getPixelColor(n - 1 - i));
        }
        TEST_ASSERT_FALSE(allBlack(strip));
    }
}

// --- edges ------------------------------------------------------------------

void test_out_of_range_samples_are_black() {
    const Strip::PixelIndex n = 30;
    FeedbackTransform tf;
    tf.zoom = 0.9f;
    Feedback fb;
    MockStrip strip(n);
    fb.begin(n, tf, 1.0f, 0);
    // Light everywhere except the centre pixels, which are the zoom's fixed
    // point and would keep sampling themselves.
    for (Strip::PixelIndex i = 0; i < n; i++) {
        if (i != 14 && i != 15) fb.addPixel(i, WHITE);
    }
    for (uint64_t t = 1; t <= 200; t++) {
        step(fb, strip, tf, 1.0f, t);
    }
    TEST_ASSERT_TRUE(allBlack(strip));
}

void test_edge_pixel_does_not_smear() {
    const Strip::PixelIndex n = 30;
    FeedbackTransform tf;
    tf.zoom = 1.1f; // inward: edge pixels sample outside the strip
    Feedback fb;
    fb.begin(n, tf, 1.0f, 0);
    fb.addPixel(0, WHITE);
    fb.addPixel(n - 1, WHITE);
    fb.begin(n, tf, 1.0f, 1);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, fb.intensity(0, 0));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, fb.intensity(n - 1, 0));
}

// --- memory -----------------------------------------------------------------

void test_buffer_size_is_three_per_pixel() {
    Feedback fb;
    TEST_ASSERT_EQUAL_UINT(0, fb.bufferSize());
    fb.begin(300, {}, 0.9f, 0);
    TEST_ASSERT_EQUAL_UINT(900, fb.bufferSize());
    TEST_ASSERT_LESS_OR_EQUAL(24u * 300u, 2u * fb.bufferSize() * sizeof(float));
}

void test_strip_length_change() {
    Feedback fb;
    MockStrip small(20);
    MockStrip large(35);
    MockStrip tiny(5);

    fb.begin(small.length(), {}, 0.9f, 0);
    fb.addPixel(10, WHITE);
    fb.render(small);

    fb.begin(large.length(), {}, 0.9f, 1);
    TEST_ASSERT_EQUAL_UINT(105, fb.bufferSize());
    fb.addPixel(34, WHITE);
    fb.render(large);
    TEST_ASSERT_EQUAL_HEX32(WHITE, large.getPixelColor(34));

    fb.begin(tiny.length(), {}, 0.9f, 2);
    TEST_ASSERT_EQUAL_UINT(15, fb.bufferSize());
    fb.addPixel(4, WHITE);
    fb.render(tiny);
    TEST_ASSERT_EQUAL_HEX32(WHITE, tiny.getPixelColor(4));
}

void test_render_blanks_pixels_beyond_the_buffer() {
    Feedback fb;
    MockStrip strip(10);
    strip.fill(WHITE);
    fb.begin(4, {}, 1.0f, 0);
    fb.render(strip);
    TEST_ASSERT_TRUE(allBlack(strip));
}

int runUnityTests() {
    UNITY_BEGIN();
    RUN_TEST(test_state_starts_black);
    RUN_TEST(test_emitted_light_persists_after_emission_stops);
    RUN_TEST(test_geometric_decay);
    RUN_TEST(test_decay_one_keeps_previous_frame);
    RUN_TEST(test_zero_decay_leaves_only_fresh_emission);
    RUN_TEST(test_trail_fades_to_true_black_within_60_iterations);
    RUN_TEST(test_overlapping_emission_saturates);
    RUN_TEST(test_add_splits_between_neighbours);
    RUN_TEST(test_add_out_of_range_is_ignored);
    RUN_TEST(test_zoom_below_one_moves_content_outward);
    RUN_TEST(test_zoom_above_one_moves_content_inward);
    RUN_TEST(test_zoom_one_is_identity);
    RUN_TEST(test_scroll_wraps_around_the_end);
    RUN_TEST(test_negative_scroll_wraps_around_the_start);
    RUN_TEST(test_zero_warp_changes_nothing);
    RUN_TEST(test_warp_moves_content);
    RUN_TEST(test_push_moves_content_outward_on_both_sides);
    RUN_TEST(test_push_clears_the_centre_pixel);
    RUN_TEST(test_push_clears_the_centre_pair_on_even_lengths);
    RUN_TEST(test_zero_push_changes_nothing);
    RUN_TEST(test_mirror_makes_output_symmetric);
    RUN_TEST(test_out_of_range_samples_are_black);
    RUN_TEST(test_edge_pixel_does_not_smear);
    RUN_TEST(test_buffer_size_is_three_per_pixel);
    RUN_TEST(test_strip_length_change);
    RUN_TEST(test_render_blanks_pixels_beyond_the_buffer);
    return UNITY_END();
}

int main() {
    return runUnityTests();
}
