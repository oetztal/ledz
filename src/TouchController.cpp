//
// Touch Controller implementation
//

#include "TouchController.h"
#include "generated/show_variants.h"
#include "Log.h"
#include "ShowController.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

static const char* const TAG = "touch";


// Solid variants come from scripts/show_variants.json via the generated header.
// Keep this in lockstep with Solid.variants[] in scripts/show_variants.json —
// the touch controller, the gallery, and ShowFactory all read from the same
// canonical JSON entry.
static const char* const SOLID_VARIANTS[] = {
    ShowVariants::kSolidVariants[0].params_json,
    ShowVariants::kSolidVariants[1].params_json,
    ShowVariants::kSolidVariants[2].params_json,
    ShowVariants::kSolidVariants[3].params_json,
    ShowVariants::kSolidVariants[4].params_json,
    ShowVariants::kSolidVariants[5].params_json,
    ShowVariants::kSolidVariants[6].params_json,
    ShowVariants::kSolidVariants[7].params_json,
    ShowVariants::kSolidVariants[8].params_json,
    ShowVariants::kSolidVariants[9].params_json,
    ShowVariants::kSolidVariants[10].params_json,
    ShowVariants::kSolidVariants[11].params_json,
    ShowVariants::kSolidVariants[12].params_json,
};

// Touch-UX-curated presets for the other touch entries. These do not appear in
// scripts/show_variants.json (the gallery) because they are curated for the
// one-button-press cycling experience on hardware, not for the gallery
// preview. Renamed TOUCH_ONLY_* to make the boundary explicit.
const char* const TOUCH_ONLY_COLORRANGES_VARIANTS[] = {
    "{\"colors\":[[0,0,255],[255,255,0]]}",
    "{\"colors\":[[255,0,0],[255,255,255],[0,255,0]]}",
    R"({"colors":[[170,21,27],[241,191,0],[170,21,27]],"ranges":[25,75]})"
};
const char* const TOUCH_ONLY_TWOCOLORBLEND_VARIANTS[] = {
    R"({"colors":[[0,0,255],[255,0,0]],"gradient":true})",
    R"({"colors":[[0,255,0],[255,0,0]],"gradient":true})",
    R"({"colors":[[0,255,0],[0,0,255]],"gradient":true})",
};
const char* const TOUCH_ONLY_COLORRUN_VARIANTS[] = {"{}"};
const char* const TOUCH_ONLY_JUMP_VARIANTS[] = {"{}"};
const char* const TOUCH_ONLY_RAINBOW_VARIANTS[] = {
    "{}",
    R"({"time_step":0.3,"pixel_step":1.0})",
    R"({"time_step":0.05,"pixel_step":0})"
};
const char* const TOUCH_ONLY_WAVE_VARIANTS[] = {"{}"};
const char* const TOUCH_ONLY_FIRE_VARIANTS[] = {
    R"({})",
    R"({"cooling":0.05})"
};
const char* const TOUCH_ONLY_STARLIGHT_VARIANTS[] = {
    R"({"probability":0.1,"length":0,"fade":250})",
    R"({"probability":0.02,"length":5000,"fade":1000})"
};
const char* const TOUCH_ONLY_THEATERCHASE_VARIANTS[] = {
    "{\"num_steps_per_cycle\":21}",
    "{\"num_steps_per_cycle\":42}",
    "{\"num_steps_per_cycle\":84}"
};
const char* const TOUCH_ONLY_MORSECODE_VARIANTS[] = {
    R"({"message":"foo bar baz"})",
    R"({"message":"gutes neues"})"
};

const TouchController::ShowVariantGroup TouchController::SHOW_VARIANTS[] = {
    {"Solid", SOLID_VARIANTS, sizeof(SOLID_VARIANTS) / sizeof(SOLID_VARIANTS[0])},
    {"Solid", TOUCH_ONLY_COLORRANGES_VARIANTS, 3},
    {"Solid", TOUCH_ONLY_TWOCOLORBLEND_VARIANTS, 3},
    {"ColorRun", TOUCH_ONLY_COLORRUN_VARIANTS, 1},
    {"Jump", TOUCH_ONLY_JUMP_VARIANTS, 1},
    {"Rainbow", TOUCH_ONLY_RAINBOW_VARIANTS, 3},
    {"Wave", TOUCH_ONLY_WAVE_VARIANTS, 1},
    {"Fire", TOUCH_ONLY_FIRE_VARIANTS, 2},
    {"Starlight", TOUCH_ONLY_STARLIGHT_VARIANTS, 2},
    {"TheaterChase", TOUCH_ONLY_THEATERCHASE_VARIANTS, 3},
    {"MorseCode", TOUCH_ONLY_MORSECODE_VARIANTS, 2}
};

const size_t TouchController::NUM_SHOW_VARIANTS = sizeof(TouchController::SHOW_VARIANTS) / sizeof(TouchController::SHOW_VARIANTS[0]);

TouchController::TouchController(Config::ConfigManager &config, ShowController &showController)
    : config(config), showController(showController), currentShowIdx(0), currentVariantIdx(0) {
    // Initialize debounce tracking
    for (uint8_t i = 0; i < Config::TouchConfig::MAX_TOUCH_PINS; i++) {
        wasTouched[i] = false;
        lastTouchTime[i] = 0;
    }
}

void TouchController::begin() {
    touchConfig = config.loadTouchConfig();
#ifdef ARDUINO
    ESP_LOGI(TAG, "Configuration loaded - enabled: %d, threshold: %d", touchConfig.enabled, touchConfig.threshold);
#endif

    // Sync currentShowIdx with the actual current show
    if (std::string currentShowName = showController.getCurrentShowName(); currentShowName.length() > 0) {
        for (size_t i = 0; i < NUM_SHOW_VARIANTS; i++) {
            if (strcmp(SHOW_VARIANTS[i].showName, currentShowName.c_str()) == 0) {
                currentShowIdx = i;
                break;
            }
        }
    }

#ifdef ARDUINO
    ESP_LOGI(TAG, "Initializing touch pins");
    for (uint8_t i = 0; i < Config::TouchConfig::MAX_TOUCH_PINS; i++) {
        const char* action = (i == 0) ? "Switch Show" : (i == 1) ? "Switch Variant" : "Switch Layout";
        ESP_LOGI(TAG, "  Touch pin %u (GPIO %u) -> %s",
                      i, TOUCH_PINS[i], action);
    }
    ESP_LOGI(TAG, "  Enabled: %s, Threshold: %u",
                  touchConfig.enabled ? "yes" : "no", touchConfig.threshold);
#endif
}

void TouchController::update() {
    if (!touchConfig.enabled) {
        return;
    }

#ifdef ARDUINO
    uint32_t now = millis();

    for (uint8_t i = 0; i < Config::TouchConfig::MAX_TOUCH_PINS; i++) {
        // Read touch value
        uint32_t touchValue = touchRead(TOUCH_PINS[i]);

        // Check if touched (value below threshold means touched)
        bool isTouched = touchValue > touchConfig.threshold;

        // Detect rising edge (transition from not-touched to touched)
        if (isTouched && !wasTouched[i]) {
            // Check debounce
            if (now - lastTouchTime[i] >= DEBOUNCE_MS) {
                lastTouchTime[i] = now;

                if (i == 0) {
                    // Button 1: Switch Show
                    currentShowIdx = (currentShowIdx + 1) % NUM_SHOW_VARIANTS;
                    currentVariantIdx = 0; // Reset to first variant of new show

                    const ShowVariantGroup& group = SHOW_VARIANTS[currentShowIdx];
                    const char* params = group.variants[currentVariantIdx];

                    ESP_LOGI(TAG, "Switching show to %s with variant %d: %s",
                                  group.showName, currentVariantIdx, params);
                    showController.queueShowChange(group.showName, params);
                } else if (i == 1) {
                    // Button 2: Switch Variant (Circulate variants of current show)
                    const ShowVariantGroup& group = SHOW_VARIANTS[currentShowIdx];
                    currentVariantIdx = (currentVariantIdx + 1) % group.numVariants;

                    const char* params = group.variants[currentVariantIdx];

                    ESP_LOGI(TAG, "Loading variant %d for show %s: %s",
                                  currentVariantIdx, group.showName, params);
                    showController.queueShowChange(group.showName, params);
                } else if (i == 2) {
                    // Button 3: Switch Layout (8 steps matching Python script)
                    static uint8_t layoutStep = 0;
                    layoutStep = (layoutStep + 1) % 8;
                    
                    bool reverse = false;
                    bool mirror = false;
                    int16_t dead_leds = 0;
                    
                    // Python create_layouts uses (dead_leds, reverse, mirror) logic:
                    // 0: (0, F, F)
                    // 1: (0, F, T)
                    // 2: (0, T, F)
                    // 3: (0, T, T)
                    // 4: (D, F, F)
                    // 5: (D, F, T)
                    // 6: (D, T, F)
                    // 7: (D, T, T)
                    // Note: Python script order is slightly different but covers same combinations
                    
                    reverse = (layoutStep % 4) >= 2;
                    mirror = (layoutStep % 2) == 1;
                    
                    if (layoutStep >= 4) {
                        Config::DeviceConfig deviceConfig = config.loadDeviceConfig();
                        // TODO: fix this
                        // dead_leds = deviceConfig.dead_leds;
                    }
                    
                    ESP_LOGI(TAG, "Switching layout to step %u (rev=%d, mir=%d, dead=%d)",
                                  layoutStep, reverse, mirror, dead_leds);
                    showController.queueLayoutChange(reverse, mirror, dead_leds);
                }
            }
        }

        wasTouched[i] = isTouched;
    }
#endif
}

void TouchController::reloadConfig() {
    touchConfig = config.loadTouchConfig();
#ifdef ARDUINO
    ESP_LOGI(TAG, "Configuration reloaded");
#endif
}

void TouchController::setTouchConfig(const Config::TouchConfig& newConfig) {
    touchConfig = newConfig;
    config.saveTouchConfig(touchConfig);
}

void TouchController::setEnabled(bool enabled) {
    touchConfig.enabled = enabled;
    config.saveTouchConfig(touchConfig);
#ifdef ARDUINO
    ESP_LOGI(TAG, "%s", enabled ? "Enabled" : "Disabled");
#endif
}

void TouchController::setThreshold(uint16_t threshold) {
    touchConfig.threshold = threshold;
    config.saveTouchConfig(touchConfig);
#ifdef ARDUINO
    ESP_LOGI(TAG, "Threshold set to %u", threshold);
#endif
}

void TouchController::getTouchValues(uint32_t* values) const {
#ifdef ARDUINO
    for (uint8_t i = 0; i < Config::TouchConfig::MAX_TOUCH_PINS; i++) {
        values[i] = touchRead(TOUCH_PINS[i]);
    }
#else
    for (uint8_t i = 0; i < Config::TouchConfig::MAX_TOUCH_PINS; i++) {
        values[i] = 0;
    }
#endif
}
