//
// Touch Controller implementation
//

#include "TouchController.h"
#include "Log.h"
#include "ShowController.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

static const char* TAG = "touch";


const char* SOLID_VARIANTS[] = {
    "{\"colors\":[[255,170,120]]}",
    "{\"colors\":[[255,255,255]]}",
    "{\"colors\":[[255,0,0]]}",
    "{\"colors\":[[255,127,0]]}",
    "{\"colors\":[[255,255,0]]}",
    "{\"colors\":[[0,255,0]]}",
    "{\"colors\":[[0,255,255]]}",
    "{\"colors\":[[0,127,255]]}",
    "{\"colors\":[[0,0,255]]}",
    "{\"colors\":[[255,0,255]]}"
};
const char* COLORRANGES_VARIANTS[] = {
    "{\"colors\":[[0,0,255],[255,255,0]]}",
    "{\"colors\":[[255,0,0],[255,255,255],[0,255,0]]}",
    R"({"colors":[[170,21,27],[241,191,0],[170,21,27]],"ranges":[25,75]})"
};
const char* TWOCOLORBLEND_VARIANTS[] = {
    R"({"colors":[[0,0,255],[255,0,0]],"gradient":true})",
    R"({"colors":[[0,255,0],[255,0,0]],"gradient":true})",
    R"({"colors":[[0,255,0],[0,0,255]],"gradient":true})",
};
const char* COLORRUN_VARIANTS[] = {"{}"};
const char* JUMP_VARIANTS[] = {"{}"};
const char* RAINBOW_VARIANTS[] = {"{}"};
const char* WAVE_VARIANTS[] = {"{}"};
const char* STARLIGHT_VARIANTS[] = {
    R"({"probability":0.1,"length":0,"fade":250})",
    R"({"probability":0.02,"length":5000,"fade":1000})"
};
const char* THEATERCHASE_VARIANTS[] = {
    "{\"num_steps_per_cycle\":21}",
    "{\"num_steps_per_cycle\":42}",
    "{\"num_steps_per_cycle\":84}"
};
const char* MORSECODE_VARIANTS[] = {
    R"({"message":"foo bar baz"})",
    R"({"message":"gutes neues"})"
};

const TouchController::ShowVariantGroup TouchController::SHOW_VARIANTS[] = {
    {"Solid", SOLID_VARIANTS, 10},
    {"Solid", COLORRANGES_VARIANTS, 3},
    {"Solid", TWOCOLORBLEND_VARIANTS, 3},
    {"ColorRun", COLORRUN_VARIANTS, 1},
    {"Jump", JUMP_VARIANTS, 1},
    {"Rainbow", RAINBOW_VARIANTS, 1},
    {"Wave", WAVE_VARIANTS, 1},
    {"Starlight", STARLIGHT_VARIANTS, 2},
    {"TheaterChase", THEATERCHASE_VARIANTS, 3},
    {"MorseCode", MORSECODE_VARIANTS, 2}
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
    std::string currentShowName = showController.getCurrentShowName();
    if (currentShowName.length() > 0) {
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
