#include "unity.h"
#include "ShowFactory.h"
#include "color.h"
#include "../MockStrip.h"
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// Exercises ShowFactory through its real JSON entry point, createShow(name,
// paramsJson). Before the ArduinoJson v7 migration ShowFactory was excluded
// from the native build, so none of this path had host coverage and two of its
// non-Arduino branches had never been compiled at all.
//
// The shows expose no accessors for their parsed parameters, so parameters are
// asserted behaviourally: run the show against a MockStrip and read back the
// pixels. ColorRanges reaches its target through a SmoothBlend driven by
// wall-clock millis() with a hardcoded 2 s duration, which a test cannot
// fast-forward. Rather than pay 2 s per scenario, every scenario is rendered
// once up front behind a single shared sleep (see renderAll), and the test
// bodies only assert against the captured pixels.

static const Strip::PixelIndex PIXELS = 10;
static const Strip::PixelIndex WIDE_PIXELS = 24;

// ColorRanges::execute steps the blend only while !isComplete(), so the final
// fade_progress == 0 step is never taken and the strip never lands exactly on
// its target colour. Sleeping past the 2 s duration therefore leaves the strip
// at whatever the *previous* step wrote, which is the initial black. So settle
// to a point comfortably inside the blend instead, and calibrate the resulting
// scale factor from a known pure-red probe (see blendScale). The margin is
// large so that scheduler jitter cannot push the sleep past the 2 s cutoff.
static const int BLEND_MS = 2000;
static const int SETTLE_MS = 1500;
static_assert(SETTLE_MS < BLEND_MS,
              "settling past the blend duration freezes the strip at its pre-final step");

struct Scenario {
    std::string params;
    Strip::PixelIndex pixelCount;
};

static std::map<std::string, std::vector<Strip::Color>> settled;

static std::string manyColorsParams() {
    std::string params = R"({"colors":[)";
    for (int i = 0; i < WIDE_PIXELS; i++) {
        if (i > 0) params += ",";
        params += "[" + std::to_string(i * 10) + ",0," + std::to_string(255 - i * 10) + "]";
    }
    return params + "]}";
}

// Render every Solid scenario concurrently so the 2 s blend is paid once.
static void renderAll() {
    ShowFactory factory;

    const std::map<std::string, Scenario> scenarios = {
        {"defaults",        {"{}", PIXELS}},
        {"no_params",       {"", PIXELS}},  // createShow(name) overload
        {"malformed",       {"{not json at all", PIXELS}},
        {"single",          {R"({"colors":[[255,0,0]]})", PIXELS}},
        {"two_even",        {R"({"colors":[[255,0,0],[0,0,255]]})", PIXELS}},
        {"two_ranges_30",   {R"({"colors":[[255,0,0],[0,0,255]],"ranges":[30]})", PIXELS}},
        {"bad_entries",     {R"({"colors":[[255,0,0],[1,2],"nope",[0,0,255]]})", PIXELS}},
        {"gradient",        {R"({"colors":[[255,0,0],[0,0,255]],"gradient":true})", PIXELS}},
        {"many",            {manyColorsParams(), WIDE_PIXELS}},
    };

    std::vector<std::unique_ptr<MockStrip>> strips;
    std::vector<std::unique_ptr<Show::Show>> shows;
    std::vector<std::string> labels;

    // Phase 1: construct and execute once. The first execute() is what builds
    // the SmoothBlend and stamps its start_time, so all blends start together.
    for (const auto &entry: scenarios) {
        auto strip = std::unique_ptr<MockStrip>(new MockStrip(entry.second.pixelCount));
        auto show = entry.second.params.empty()
                        ? factory.createShow("Solid")
                        : factory.createShow("Solid", entry.second.params);
        if (show == nullptr) {
            continue;  // asserted separately; skip rather than crash here
        }
        show->execute(*strip, 0);
        labels.push_back(entry.first);
        shows.push_back(std::move(show));
        strips.push_back(std::move(strip));
    }

    // Phase 2: one sleep for all of them.
    std::this_thread::sleep_for(std::chrono::milliseconds(SETTLE_MS));

    // Phase 3: one more step, now near the end of the blend, so each strip
    // holds its target colour scaled by the shared remaining progress.
    for (size_t i = 0; i < shows.size(); i++) {
        shows[i]->execute(*strips[i], 1);
        std::vector<Strip::Color> pixels;
        for (Strip::PixelIndex p = 0; p < strips[i]->length(); p++) {
            pixels.push_back(strips[i]->getPixelColor(p));
        }
        settled[labels[i]] = pixels;
    }
}

static const std::vector<Strip::Color> &pixelsOf(const std::string &label) {
    auto it = settled.find(label);
    TEST_ASSERT_TRUE_MESSAGE(it != settled.end(), label.c_str());
    return it->second;
}

// The blend is linear, so every captured channel is target * (1 - progress)
// with the same factor across all scenarios — they share one sleep. Recover
// that factor from the pure-red scenario rather than deriving it from the
// clock, which makes the assertions immune to timing jitter.
static float blendScale() {
    return static_cast<float>(red(pixelsOf("single")[0])) / 255.0f;
}

// Assert a captured pixel matches the requested colour once the shared blend
// scale is applied. A zero channel must stay exactly zero: the strip starts
// black, so 0 * anything is 0 regardless of progress.
static void assertColor(Strip::Color expected, Strip::Color actual, const char *what) {
    const float s = blendScale();
    const Strip::ColorComponent ec[3] = {red(expected), green(expected), blue(expected)};
    const Strip::ColorComponent ac[3] = {red(actual), green(actual), blue(actual)};

    for (int c = 0; c < 3; c++) {
        if (ec[c] == 0) {
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, ac[c], what);
        } else {
            TEST_ASSERT_UINT8_WITHIN_MESSAGE(
                3, static_cast<Strip::ColorComponent>(ec[c] * s), ac[c], what);
        }
    }
}

// The blend must actually have moved, or every assertion above would pass
// vacuously against an all-black strip.
static void assertBlendProgressed() {
    TEST_ASSERT_TRUE_MESSAGE(blendScale() > 0.3f,
                             "blend did not progress; SETTLE_MS may exceed the blend duration");
}

static ShowFactory *factory;

void setUp() {
    factory = new ShowFactory();
}

void tearDown() {
    delete factory;
}

// --- registration -----------------------------------------------------------

void test_all_shows_are_registered() {
    const char *expected[] = {
        "Solid", "Fire", "Starlight", "Stroboscope", "ColorRun", "Jump",
        "Rainbow", "Wave", "TheaterChase", "MorseCode", "Chaos", "Mandelbrot"
    };
    const size_t count = sizeof(expected) / sizeof(expected[0]);

    TEST_ASSERT_EQUAL(count, factory->listShows().size());
    for (size_t i = 0; i < count; i++) {
        TEST_ASSERT_TRUE_MESSAGE(factory->hasShow(expected[i]), expected[i]);
    }
}

void test_every_registered_show_constructs_with_empty_params() {
    for (const auto &info: factory->listShows()) {
        auto show = factory->createShow(info.name, "{}");
        TEST_ASSERT_NOT_NULL_MESSAGE(show.get(), info.name.c_str());
    }
}

void test_unknown_show_returns_null() {
    TEST_ASSERT_FALSE(factory->hasShow("NoSuchShow"));
    auto show = factory->createShow("NoSuchShow", "{}");
    TEST_ASSERT_NULL(show.get());
}

void test_show_list_entries_have_descriptions() {
    for (const auto &info: factory->listShows()) {
        TEST_ASSERT_FALSE(info.name.empty());
        TEST_ASSERT_FALSE_MESSAGE(info.description.empty(), info.name.c_str());
    }
}

// --- malformed input falls back to defaults ---------------------------------

void test_malformed_json_still_constructs_every_show() {
    // createShow logs and clears the document, so every parameter falls back
    // to its default via the | operator rather than the request failing.
    for (const auto &info: factory->listShows()) {
        auto show = factory->createShow(info.name, "{\"colors\":");
        TEST_ASSERT_NOT_NULL_MESSAGE(show.get(), info.name.c_str());
    }
}

void test_malformed_json_yields_the_default_appearance() {
    TEST_ASSERT_EQUAL_HEX32_ARRAY(pixelsOf("defaults").data(),
                                  pixelsOf("malformed").data(), PIXELS);
}

void test_createShow_without_params_matches_empty_object() {
    TEST_ASSERT_EQUAL_HEX32_ARRAY(pixelsOf("defaults").data(),
                                  pixelsOf("no_params").data(), PIXELS);
}

void test_empty_params_give_warm_white() {
    assertBlendProgressed();
    const auto &px = pixelsOf("defaults");
    for (Strip::PixelIndex i = 0; i < PIXELS; i++) {
        assertColor(color(255, 250, 230), px[i], "warm white default");
    }
}

// --- Solid / ColorRanges parameter parsing ----------------------------------

void test_single_color_fills_the_strip() {
    assertBlendProgressed();
    const auto &px = pixelsOf("single");
    for (Strip::PixelIndex i = 0; i < PIXELS; i++) {
        assertColor(color(255, 0, 0), px[i], "single colour");
    }
}

void test_two_colors_split_the_strip_evenly() {
    assertBlendProgressed();
    const auto &px = pixelsOf("two_even");
    for (Strip::PixelIndex i = 0; i < PIXELS / 2; i++) {
        assertColor(color(255, 0, 0), px[i], "first half red");
    }
    for (Strip::PixelIndex i = PIXELS / 2; i < PIXELS; i++) {
        assertColor(color(0, 0, 255), px[i], "second half blue");
    }
}

void test_ranges_move_the_boundary() {
    // 30% boundary: pixels 0-2 red, 3-9 blue.
    assertBlendProgressed();
    const auto &px = pixelsOf("two_ranges_30");
    assertColor(color(255, 0, 0), px[0], "pixel 0 red");
    assertColor(color(255, 0, 0), px[2], "pixel 2 red");
    assertColor(color(0, 0, 255), px[3], "pixel 3 blue");
    assertColor(color(0, 0, 255), px[9], "pixel 9 blue");
}

void test_malformed_color_entries_are_skipped() {
    // Entries with fewer than three components, and non-array entries, are
    // ignored; the two valid colours still split the strip.
    assertBlendProgressed();
    const auto &px = pixelsOf("bad_entries");
    assertColor(color(255, 0, 0), px[0], "first valid colour");
    assertColor(color(0, 0, 255), px[PIXELS - 1], "last valid colour");
}

// The payload that motivated the JSON_DOC_LARGE parse buffer at the old
// ShowFactory.cpp:219 and, under v6, the silent re-copy into the
// JSON_DOC_MEDIUM constructor signature. Under v7 the document is elastic, so
// every colour must survive regardless of how long the parameter string is.
void test_many_colors_are_all_parsed() {
    assertBlendProgressed();
    const auto &px = pixelsOf("many");
    TEST_ASSERT_EQUAL(WIDE_PIXELS, px.size());
    for (int i = 0; i < WIDE_PIXELS; i++) {
        assertColor(color(i * 10, 0, 255 - i * 10), px[i], "gradient stop");
    }
}

void test_gradient_flag_changes_the_result() {
    const auto &sharp = pixelsOf("two_even");
    const auto &blended = pixelsOf("gradient");

    bool differs = false;
    for (Strip::PixelIndex i = 0; i < PIXELS; i++) {
        if (sharp[i] != blended[i]) {
            differs = true;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(differs, "gradient:true produced identical pixels");
}

int runUnityTests() {
    renderAll();

    UNITY_BEGIN();

    RUN_TEST(test_all_shows_are_registered);
    RUN_TEST(test_every_registered_show_constructs_with_empty_params);
    RUN_TEST(test_unknown_show_returns_null);
    RUN_TEST(test_show_list_entries_have_descriptions);

    RUN_TEST(test_malformed_json_still_constructs_every_show);
    RUN_TEST(test_malformed_json_yields_the_default_appearance);
    RUN_TEST(test_createShow_without_params_matches_empty_object);
    RUN_TEST(test_empty_params_give_warm_white);

    RUN_TEST(test_single_color_fills_the_strip);
    RUN_TEST(test_two_colors_split_the_strip_evenly);
    RUN_TEST(test_ranges_move_the_boundary);
    RUN_TEST(test_malformed_color_entries_are_skipped);
    RUN_TEST(test_many_colors_are_all_parsed);
    RUN_TEST(test_gradient_flag_changes_the_result);

    return UNITY_END();
}

int main() {
    return runUnityTests();
}
