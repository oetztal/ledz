#include <cmath>

#include "unity.h"
#include "../MockStrip.h"
#include "show/Fire.h"
#include "show/Rainbow.h"
#include "show/Wave.h"
#include "show/Tunnel.h"
#include "show/Echo.h"
#include "support/Random.h"
#include "support/Color.h"

Show::FireState* state;

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
        TEST_ASSERT_EQUAL_HEX32_MESSAGE(strip_t.getPixelColor(i), strip_b.getPixelColor(i),
                                        "bounce and traveling modes must produce identical pixels");
    }
}

// Tunnel show tests
static int brightness(Strip::Color c) {
    return Support::Color::red(c) + Support::Color::green(c) + Support::Color::blue(c);
}

static bool isSymmetric(const MockStrip& strip) {
    const Strip::PixelIndex n = strip.length();
    for (Strip::PixelIndex i = 0; i < n / 2; i++) {
        if (strip.getPixelColor(i) != strip.getPixelColor(n - 1 - i)) return false;
    }
    return true;
}

void test_tunnel_centre_lit_on_first_iteration() {
    for (Strip::PixelIndex n : {1, 2, 7, 60, 61}) {
        Show::Tunnel show(0.97f, 0.93f, 2.5f, 0.0f, 1.0f, 20, 0.0f);
        MockStrip strip(n);
        show.execute(strip, 0);
        TEST_ASSERT_NOT_EQUAL_MESSAGE(0, strip.getPixelColor((n - 1) / 2), "centre pixel is black");
        TEST_ASSERT_NOT_EQUAL_MESSAGE(0, strip.getPixelColor(n / 2), "centre pixel is black");
    }
}

void test_tunnel_hue_changes_over_time() {
    Show::Tunnel show(0.97f, 0.93f, 2.5f, 0.0f, 1.0f, 20, 0.0f);
    MockStrip strip(60);
    show.execute(strip, 0);
    const Strip::Color first = strip.getPixelColor(30);
    for (Show::Iteration t = 1; t <= 32; t++) {
        show.execute(strip, t);
    }
    TEST_ASSERT_NOT_EQUAL(first, strip.getPixelColor(30));
}

void test_tunnel_reaches_both_ends_with_defaults() {
    Show::Tunnel show(0.97f, 0.93f, 2.5f, 0.0f, 1.0f, 20, 0.0f);
    MockStrip strip(60);
    bool first_lit = false;
    bool last_lit = false;
    for (Show::Iteration t = 0; t < 200; t++) {
        show.execute(strip, t);
        first_lit = first_lit || strip.getPixelColor(0) != 0;
        last_lit = last_lit || strip.getPixelColor(59) != 0;
    }
    TEST_ASSERT_TRUE_MESSAGE(first_lit, "pixel 0 never lit");
    TEST_ASSERT_TRUE_MESSAGE(last_lit, "pixel 59 never lit");
}

void test_tunnel_slow_push_still_reaches_both_ends() {
    Show::Tunnel show(0.97f, 0.97f, 2.5f, 0.0f, 0.25f, 20, 0.0f);
    MockStrip strip(60);
    bool first_lit = false;
    bool last_lit = false;
    for (Show::Iteration t = 0; t < 400; t++) {
        show.execute(strip, t);
        first_lit = first_lit || strip.getPixelColor(0) != 0;
        last_lit = last_lit || strip.getPixelColor(59) != 0;
    }
    TEST_ASSERT_TRUE_MESSAGE(first_lit, "pixel 0 never lit with slow push");
    TEST_ASSERT_TRUE_MESSAGE(last_lit, "pixel 59 never lit with slow push");
}

void test_tunnel_decay_is_per_pixel() {
    Show::Tunnel show(0.97f, 0.97f, 2.5f, 0.0f, 0.5f, 20, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, std::pow(0.97f, 0.5f), show.frameDecay());
}

void test_tunnel_zero_push_keeps_per_frame_decay() {
    Show::Tunnel show(0.97f, 0.9f, 2.5f, 0.0f, 0.0f, 20, 0.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.9f, show.frameDecay());
}

void test_tunnel_pulse_phase_follows_distance() {
    Show::Tunnel show(0.97f, 0.97f, 2.5f, 0.0f, 0.5f, 20, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, show.pulseIntensity(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, show.pulseIntensity(20.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, show.pulseIntensity(40.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.08f, show.pulseIntensity(10.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.08f, show.pulseIntensity(30.0f));
}

void test_tunnel_emission_distance_accumulates_push() {
    Show::Tunnel show(0.97f, 0.97f, 2.5f, 0.0f, 0.25f, 20, 0.0f);
    MockStrip strip(61);
    for (Show::Iteration t = 0; t < 400; t++) {
        show.execute(strip, t);
    }
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 100.0f, show.emissionDistance());
}

void test_tunnel_slow_push_keeps_ring_spacing() {
    Show::Tunnel fast(1.0f, 0.97f, 0.0f, 0.0f, 1.0f, 20, 0.0f);
    Show::Tunnel slow(1.0f, 0.97f, 0.0f, 0.0f, 0.25f, 20, 0.0f);
    MockStrip fast_strip(121);
    MockStrip slow_strip(121);
    // Stop at 110 px of travel, the pulse trough, so the fresh flash does not
    // drown out the rings emitted earlier.
    for (Show::Iteration t = 0; t < 110; t++) {
        fast.execute(fast_strip, t);
    }
    for (Show::Iteration t = 0; t < 440; t++) {
        slow.execute(slow_strip, t);
    }

    // After 110 px of travel the ring emitted at 80 px sits at radius 30. Both
    // tunnels should have a local maximum near there; the fresh flash sits too
    // close to the centre to search from.
    auto ringRadius = [](const MockStrip& strip) {
        const Strip::PixelIndex centre = strip.length() / 2;
        Strip::PixelIndex best = 25;
        for (Strip::PixelIndex r = 26; r <= 35; r++) {
            if (brightness(strip.getPixelColor(centre - r)) > brightness(strip.getPixelColor(centre - best))) {
                best = r;
            }
        }
        return best;
    };
    TEST_ASSERT_INT_WITHIN(2, 30, ringRadius(fast_strip));
    TEST_ASSERT_INT_WITHIN(2, 30, ringRadius(slow_strip));
}

void test_tunnel_zero_push_pulses_over_time() {
    Show::Tunnel show(0.97f, 0.9f, 0.0f, 0.0f, 0.0f, 20, 0.0f);
    MockStrip strip(61);
    int centre[21];
    for (Show::Iteration t = 0; t <= 20; t++) {
        show.execute(strip, t);
        centre[t] = brightness(strip.getPixelColor(30));
    }
    TEST_ASSERT_GREATER_THAN(centre[10], centre[0]);
    TEST_ASSERT_GREATER_THAN(centre[10], centre[20]);
}

void test_tunnel_symmetric_without_warp_or_sway() {
    for (Strip::PixelIndex n : {60, 61}) {
        Show::Tunnel show(0.9f, 0.97f, 2.0f, 0.0f, 1.5f, 7, 0.0f);
        MockStrip strip(n);
        for (Show::Iteration t = 0; t < 200; t++) {
            show.execute(strip, t);
            TEST_ASSERT_TRUE_MESSAGE(isSymmetric(strip), "asymmetric frame without warp or sway");
        }
    }
}

void test_tunnel_warp_breaks_symmetry() {
    Show::Tunnel show(0.97f, 0.93f, 2.5f, 2.0f, 1.0f, 20, 0.0f);
    MockStrip strip(60);
    bool asymmetric = false;
    for (Show::Iteration t = 0; t < 100; t++) {
        show.execute(strip, t);
        asymmetric = asymmetric || !isSymmetric(strip);
    }
    TEST_ASSERT_TRUE(asymmetric);
}

void test_tunnel_pulses_form_rings() {
    Show::Tunnel show(0.97f, 0.93f, 0.0f, 0.0f, 1.0f, 20, 0.0f);
    MockStrip strip(61);
    int centre[21];
    for (Show::Iteration t = 0; t <= 20; t++) {
        show.execute(strip, t);
        centre[t] = brightness(strip.getPixelColor(30));
    }
    TEST_ASSERT_GREATER_THAN(centre[10], centre[0]);
    TEST_ASSERT_GREATER_THAN(centre[10], centre[20]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, show.pulseIntensity(0));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.08f, show.pulseIntensity(10));
}

void test_tunnel_continuous_without_pulse() {
    Show::Tunnel show(0.97f, 0.93f, 0.0f, 0.0f, 1.0f, 0, 0.0f);
    MockStrip strip(61);
    for (Show::Iteration t = 0; t < 50; t++) {
        show.execute(strip, t);
        TEST_ASSERT_EQUAL_FLOAT(1.0f, show.pulseIntensity(static_cast<uint32_t>(t)));
        TEST_ASSERT_EQUAL_HEX32(Support::Color::wheel(0.0f), strip.getPixelColor(30));
    }
}

void test_tunnel_even_length_centre_pair_does_not_accumulate() {
    Show::Tunnel show(0.97f, 0.93f, 0.0f, 0.0f, 1.0f, 20, 0.0f);
    MockStrip strip(60);
    for (Show::Iteration t = 0; t <= 10; t++) {
        show.execute(strip, t);
    }
    // Frame 10 is the pulse trough. With hue_speed 0 the emission is
    // wheel(0) at 0.08 intensity, so each centre pixel's red channel should
    // be about 20 rather than the saturated glow an accumulating pair gives.
    for (Strip::PixelIndex i : {29, 30}) {
        TEST_ASSERT_INT_WITHIN(1, 20, Support::Color::red(strip.getPixelColor(i)));
        TEST_ASSERT_EQUAL_UINT8(0, Support::Color::green(strip.getPixelColor(i)));
    }
}

void test_tunnel_sway_breaks_symmetry() {
    Show::Tunnel show(0.97f, 0.93f, 2.5f, 0.0f, 1.0f, 20, 0.4f);
    MockStrip strip(60);
    bool asymmetric = false;
    for (Show::Iteration t = 0; t < 100; t++) {
        show.execute(strip, t);
        asymmetric = asymmetric || !isSymmetric(strip);
    }
    TEST_ASSERT_TRUE(asymmetric);
}

void test_tunnel_sway_moves_the_brightest_pixel() {
    Show::Tunnel show(0.97f, 0.93f, 0.0f, 0.0f, 1.0f, 0, 1.0f);
    MockStrip strip(61);
    bool left = false;
    bool right = false;
    for (Show::Iteration t = 0; t < 300; t++) {
        show.execute(strip, t);
        Strip::PixelIndex brightest = 0;
        for (Strip::PixelIndex i = 1; i < strip.length(); i++) {
            if (brightness(strip.getPixelColor(i)) > brightness(strip.getPixelColor(brightest))) brightest = i;
        }
        left = left || brightest < 28;
        right = right || brightest > 32;
    }
    TEST_ASSERT_TRUE_MESSAGE(left, "centre never drifted left");
    TEST_ASSERT_TRUE_MESSAGE(right, "centre never drifted right");
}

void test_tunnel_sway_offset_stays_within_bounds() {
    Show::Tunnel fixed(0.97f, 0.93f, 2.5f, 0.0f, 1.0f, 20, 0.0f);
    Show::Tunnel swaying(0.97f, 0.93f, 2.5f, 0.0f, 1.0f, 20, 0.5f);
    for (uint32_t f = 0; f < 5000; f += 7) {
        TEST_ASSERT_EQUAL_FLOAT(0.0f, fixed.swayOffset(f, 61));
        TEST_ASSERT_TRUE(std::fabs(swaying.swayOffset(f, 61)) <= 0.5f * 30.0f + 1e-4f);
    }
}

// Echo show tests
void test_echo_particles_stay_on_the_strip() {
    struct Case {
        int particles;
        float speed;
        Strip::PixelIndex n;
    };
    for (const Case& c :
         {Case{3, 0.5f, 60}, Case{8, 4.0f, 60}, Case{8, 4.0f, 3}, Case{2, 3.7f, 1}, Case{1, 0.05f, 10}}) {
        Show::Echo show(c.particles, c.speed, 0.92f, 0.2f);
        MockStrip strip(c.n);
        for (Show::Iteration t = 0; t < 1000; t++) {
            show.execute(strip, t);
            TEST_ASSERT_EQUAL(c.particles, show.particles().size());
            for (const auto& p : show.particles()) {
                TEST_ASSERT_TRUE(p.pos >= 0.0f);
                TEST_ASSERT_TRUE(p.pos <= static_cast<float>(c.n - 1));
            }
        }
    }
}

void test_echo_is_deterministic_with_a_seed() {
    MockStrip a(60);
    MockStrip b(60);
    Support::setRandomSeedOverride(1234);
    Show::Echo first(3, 0.5f, 0.92f, 0.2f);
    Show::Echo second(3, 0.5f, 0.92f, 0.2f);
    for (Show::Iteration t = 0; t < 300; t++) {
        first.execute(a, t);
        second.execute(b, t);
        for (Strip::PixelIndex i = 0; i < 60; i++) {
            TEST_ASSERT_EQUAL_HEX32(a.getPixelColor(i), b.getPixelColor(i));
        }
    }
    Support::clearRandomSeedOverride();
}

void test_echo_two_particles_have_opposite_hues() {
    Show::Echo show(2, 0.5f, 0.92f, 0.0f);
    MockStrip strip(60);
    for (Show::Iteration t = 0; t < 10; t++) {
        show.execute(strip, t);
        TEST_ASSERT_EQUAL_HEX32(Support::Color::wheel(0.0f), show.particleColor(0));
        TEST_ASSERT_EQUAL_HEX32(Support::Color::wheel(128.0f), show.particleColor(1));
    }
}

void test_echo_leaves_a_decaying_trail() {
    Support::setRandomSeedOverride(7);
    Show::Echo show(1, 1.0f, 0.9f, 0.2f);
    MockStrip strip(60);
    Show::Iteration t = 0;
    // Run 20 iterations, continuing until the particle is far enough from an
    // end that the trail behind it has not been folded by a recent bounce.
    const auto clear_of_ends = [&] {
        const auto& p = show.particles()[0];
        return p.pos >= 8.0f && p.pos <= 51.0f;
    };
    while (t < 20 || !clear_of_ends()) {
        show.execute(strip, t++);
        TEST_ASSERT_LESS_THAN(200, t);
    }
    Support::clearRandomSeedOverride();

    int lit = 0;
    for (Strip::PixelIndex i = 0; i < 60; i++) {
        lit += strip.getPixelColor(i) != 0 ? 1 : 0;
    }
    TEST_ASSERT_GREATER_OR_EQUAL(5, lit);

    // Walk backwards from the first pixel fully behind the particle (the one
    // under it has only received part of this frame's light): brightness
    // must fall off with distance.
    const auto& p = show.particles()[0];
    const auto step = static_cast<Strip::PixelIndex>(-p.dir);
    auto i = static_cast<Strip::PixelIndex>((p.dir > 0 ? std::floor(p.pos) : std::ceil(p.pos)) + step);
    int previous = brightness(strip.getPixelColor(i));
    for (int k = 1; k <= 5; k++) {
        i = static_cast<Strip::PixelIndex>(i + step);
        const int current = brightness(strip.getPixelColor(i));
        TEST_ASSERT_GREATER_THAN(0, current);
        TEST_ASSERT_LESS_THAN(previous, current);
        previous = current;
    }
}

void test_echo_particles_move_at_different_speeds() {
    Show::Echo show(3, 1.0f, 0.92f, 0.2f);
    MockStrip strip(300);
    show.execute(strip, 0);
    const float expected[] = {0.6f, 1.0f, 1.4f};
    for (int k = 0; k < 20; k++) {
        const auto before = show.particles();
        show.execute(strip, k + 1);
        for (std::size_t j = 0; j < 3; j++) {
            TEST_ASSERT_FLOAT_WITHIN(1e-6f, expected[j], show.particles()[j].speed);
            const float travelled = std::fabs(show.particles()[j].pos - before[j].pos);
            const bool bounced = show.particles()[j].dir != before[j].dir;
            if (!bounced) {
                TEST_ASSERT_FLOAT_WITHIN(1e-3f, expected[j], travelled);
            }
        }
    }
}

void test_echo_trail_has_no_stripes_at_fractional_speed() {
    Support::setRandomSeedOverride(42);
    Show::Echo show(1, 0.8f, 1.0f, 0.0f);
    MockStrip strip(300);
    show.execute(strip, 0);
    Support::clearRandomSeedOverride();

    const float start = show.particles()[0].pos;
    const float dir = show.particles()[0].dir;
    // Precondition: 20 px of travel stay clear of both ends.
    TEST_ASSERT_TRUE(start + dir * 21.0f > 1.0f && start + dir * 21.0f < 298.0f);
    Show::Iteration t = 1;
    while (std::fabs(show.particles()[0].pos - start) < 20.0f) {
        show.execute(strip, t++);
    }
    const float end = show.particles()[0].pos;
    // Pixels strictly inside the travelled path, more than one pixel behind
    // the particle and clear of the partially lit starting pixel.
    const float lo = std::min(start, end - dir) + 1.0f;
    const float hi = std::max(start, end - dir) - 1.0f;
    int checked = 0;
    for (auto i = static_cast<Strip::PixelIndex>(std::ceil(lo)); i <= static_cast<Strip::PixelIndex>(hi); i++) {
        TEST_ASSERT_GREATER_OR_EQUAL(250, Support::Color::red(strip.getPixelColor(i)));
        TEST_ASSERT_EQUAL_UINT8(0, Support::Color::green(strip.getPixelColor(i)));
        checked++;
    }
    TEST_ASSERT_GREATER_OR_EQUAL(15, checked);
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

    // Tunnel show
    RUN_TEST(test_tunnel_centre_lit_on_first_iteration);
    RUN_TEST(test_tunnel_hue_changes_over_time);
    RUN_TEST(test_tunnel_reaches_both_ends_with_defaults);
    RUN_TEST(test_tunnel_slow_push_still_reaches_both_ends);
    RUN_TEST(test_tunnel_decay_is_per_pixel);
    RUN_TEST(test_tunnel_zero_push_keeps_per_frame_decay);
    RUN_TEST(test_tunnel_pulse_phase_follows_distance);
    RUN_TEST(test_tunnel_emission_distance_accumulates_push);
    RUN_TEST(test_tunnel_slow_push_keeps_ring_spacing);
    RUN_TEST(test_tunnel_zero_push_pulses_over_time);
    RUN_TEST(test_tunnel_symmetric_without_warp_or_sway);
    RUN_TEST(test_tunnel_warp_breaks_symmetry);
    RUN_TEST(test_tunnel_sway_breaks_symmetry);
    RUN_TEST(test_tunnel_sway_moves_the_brightest_pixel);
    RUN_TEST(test_tunnel_sway_offset_stays_within_bounds);
    RUN_TEST(test_tunnel_pulses_form_rings);
    RUN_TEST(test_tunnel_continuous_without_pulse);
    RUN_TEST(test_tunnel_even_length_centre_pair_does_not_accumulate);

    // Echo show
    RUN_TEST(test_echo_particles_stay_on_the_strip);
    RUN_TEST(test_echo_is_deterministic_with_a_seed);
    RUN_TEST(test_echo_two_particles_have_opposite_hues);
    RUN_TEST(test_echo_leaves_a_decaying_trail);
    RUN_TEST(test_echo_particles_move_at_different_speeds);
    RUN_TEST(test_echo_trail_has_no_stripes_at_fractional_speed);

    return UNITY_END();
}

int main() {
    return runUnityTests();
}
