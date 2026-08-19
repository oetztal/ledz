#include "ShowController.h"
#include "Log.h"
#include "color.h"

#include <cstring> // strlen, strncpy

#ifdef ARDUINO
#include <Arduino.h>

// Queue size for show commands between cores
static constexpr size_t SHOW_COMMAND_QUEUE_SIZE = 5;
#endif

static const char* TAG = "ctrl";

ShowController::ShowController(ShowFactory &factory, Config::ConfigManager &config)
    : factory(factory), config(config), brightness(128),
      layout(), baseStrip()
#ifdef ARDUINO
      , commandQueue(nullptr)
#endif
{
}

void ShowController::begin() {
#ifdef ARDUINO
    // Create FreeRTOS queue sized by SHOW_COMMAND_QUEUE_SIZE
    commandQueue = xQueueCreate(SHOW_COMMAND_QUEUE_SIZE, sizeof(ShowCommand));

    if (commandQueue == nullptr) {
        ESP_LOGE(TAG, "Failed to create show command queue!");
        return;
    }
#endif
    Config::ShowConfig showConfig = config.loadShowConfig();
    Config::DeviceConfig deviceConfig = config.loadDeviceConfig();

    // Apply loaded configuration
    brightness = deviceConfig.brightness;

#ifdef ARDUINO
    ESP_LOGI(TAG, "preparing show");
#endif
    // Create initial show
    const char *initialShowName = showConfig.current_show;

    if (strlen(initialShowName) > 0 && factory.hasShow(initialShowName)) {
        currentShowName = initialShowName;
    } else {
        currentShowName = "Rainbow";
    }

    // Load parameters if available
    const char *params = (strlen(showConfig.params_json) > 0) ? showConfig.params_json : "{}";
#ifdef ARDUINO
    ESP_LOGI(TAG, "Creating initial show %s with params %s", currentShowName.c_str(), params);
#endif
    currentShow = factory.createShow(currentShowName, params);
#ifdef ARDUINO
    ESP_LOGD(TAG, "Show %s created", currentShowName.c_str());
    ESP_LOGD(TAG, "Initial show %p", currentShow.get());
#endif

    if (currentShow) {
#ifdef ARDUINO
        ESP_LOGI(TAG, "Initial show loaded: %s with params: %s",
                       currentShowName.c_str(), params);
#endif
    } else {
#ifdef ARDUINO
        ESP_LOGE(TAG, "Failed to create initial show!");
#endif
    }
}

bool ShowController::queueShowChange(const std::string &showName, const std::string &paramsJson) {
#ifdef ARDUINO
    if (commandQueue == nullptr) {
        return false;
    }

    ShowCommand cmd;
    cmd.type = ShowCommandType::SET_SHOW;
    cmd.show_name = strdup(showName.c_str());
    cmd.params_json = strdup(paramsJson.c_str());

    // Try to send with no wait (non-blocking)
    if (xQueueSend(commandQueue, &cmd, 0) == pdTRUE) {
        return true;
    }

    // If failed to queue, we must free the memory
    free(cmd.show_name);
    free(cmd.params_json);

    ESP_LOGW(TAG, "Show command queue full!");
    return false;
#else
    return false;
#endif
}

bool ShowController::queueBrightnessChange(uint8_t brightness) {
#ifdef ARDUINO
    if (commandQueue == nullptr) {
        return false;
    }

    ShowCommand cmd;
    cmd.type = ShowCommandType::SET_BRIGHTNESS;
    cmd.brightness_value = brightness;

    if (xQueueSend(commandQueue, &cmd, 0) == pdTRUE) {
        return true;
    }

    ESP_LOGW(TAG, "Brightness command queue full!");
    return false;
#else
    return false;
#endif
}

void ShowController::applyCommand(const ShowCommand &cmd) {
    switch (cmd.type) {
        case ShowCommandType::SET_SHOW: {
            // Create new show with parameters
            std::unique_ptr<Show::Show> newShow = factory.createShow(cmd.show_name, cmd.params_json);
            if (newShow != nullptr) {
                currentShow = std::move(newShow);
                {
                    std::lock_guard<std::mutex> lock(stateMutex);
                    currentShowName = cmd.show_name;
                }

#ifdef ARDUINO
                ESP_LOGI(TAG, "Switched to show: %s with params: %s",
                               currentShowName.c_str(), cmd.params_json);
#endif

                // Save to configuration
                Config::ShowConfig showConfig = config.loadShowConfig();
                strncpy(showConfig.current_show, currentShowName.c_str(), sizeof(showConfig.current_show) - 1);
                showConfig.current_show[sizeof(showConfig.current_show) - 1] = '\0';
                strncpy(showConfig.params_json, cmd.params_json, sizeof(showConfig.params_json) - 1);
                showConfig.params_json[sizeof(showConfig.params_json) - 1] = '\0';
                config.saveShowConfig(showConfig);
            } else {
#ifdef ARDUINO
                ESP_LOGE(TAG, "Failed to create show: %s", cmd.show_name);
#endif
            }
            break;
        }

        case ShowCommandType::SET_BRIGHTNESS: {
            brightness.store(cmd.brightness_value);

#ifdef ARDUINO
            ESP_LOGI(TAG, "Brightness set to: %u", brightness.load());
#endif

            // Save to configuration
            Config::DeviceConfig deviceConfig = config.loadDeviceConfig();
            deviceConfig.brightness = brightness.load();
            config.saveDeviceConfig(deviceConfig);
            break;
        }

        case ShowCommandType::SET_LAYOUT: {
#ifdef ARDUINO
            if (layout != nullptr && baseStrip != nullptr) {
                // Recreate layout with new parameters
                layout = std::make_unique<Strip::Layout>(*baseStrip, cmd.layout_reverse,
                                                         cmd.layout_mirror, cmd.layout_dead_leds);

                ESP_LOGI(TAG, "Layout updated - reverse=%d, mirror=%d, dead_leds=%u",
                              cmd.layout_reverse, cmd.layout_mirror, cmd.layout_dead_leds);

                // Save to configuration
                Config::LayoutConfig layoutConfig;
                layoutConfig.reverse = cmd.layout_reverse;
                layoutConfig.mirror = cmd.layout_mirror;
                layoutConfig.dead_leds = cmd.layout_dead_leds;
                config.saveLayoutConfig(layoutConfig);

                // Restart current show to pick up new layout dimensions
                Config::ShowConfig showConfig = config.loadShowConfig();
                std::unique_ptr<Show::Show> newShow = factory.createShow(currentShowName, showConfig.params_json);
                if (newShow != nullptr) {
                    currentShow = std::move(newShow);
                    ESP_LOGI(TAG, "Restarted show '%s' with updated layout", currentShowName.c_str());
                }
            } else {
                ESP_LOGE(TAG, "Layout pointers not set!");
            }
#endif
            break;
        }

        case ShowCommandType::LOAD_PRESET: {
#ifdef ARDUINO
            ESP_LOGI(TAG, "Loading preset - show=%s", cmd.show_name);

            // 1. Update layout if we have valid strip pointers
            if (layout != nullptr && baseStrip != nullptr) {
                layout = std::make_unique<Strip::Layout>(*baseStrip, cmd.layout_reverse,
                                                         cmd.layout_mirror, cmd.layout_dead_leds);

                ESP_LOGD(TAG, "Preset layout - reverse=%d, mirror=%d, dead_leds=%d",
                              cmd.layout_reverse, cmd.layout_mirror, cmd.layout_dead_leds);

                // Save layout config
                Config::LayoutConfig layoutConfig;
                layoutConfig.reverse = cmd.layout_reverse;
                layoutConfig.mirror = cmd.layout_mirror;
                layoutConfig.dead_leds = cmd.layout_dead_leds;
                config.saveLayoutConfig(layoutConfig);
            }

            // 2. Create show with preset parameters
            std::unique_ptr<Show::Show> newShow = factory.createShow(cmd.show_name, cmd.params_json);
            if (newShow != nullptr) {
                currentShow = std::move(newShow);
                {
                    std::lock_guard<std::mutex> lock(stateMutex);
                    currentShowName = cmd.show_name;
                }

                ESP_LOGI(TAG, "Preset show '%s' loaded with params: %s",
                              currentShowName.c_str(), cmd.params_json);

                // Save show config
                Config::ShowConfig showConfig = config.loadShowConfig();
                strncpy(showConfig.current_show, currentShowName.c_str(), sizeof(showConfig.current_show) - 1);
                strncpy(showConfig.params_json, cmd.params_json, sizeof(showConfig.params_json) - 1);
                config.saveShowConfig(showConfig);
            } else {
                ESP_LOGE(TAG, "Failed to create preset show: %s", cmd.show_name);
            }
#endif
            break;
        }
    }
}

void ShowController::processCommands() {
#ifdef ARDUINO
    if (commandQueue == nullptr) {
        return;
    }

    // Process all pending commands (non-blocking)
    ShowCommand cmd;
    while (xQueueReceive(commandQueue, &cmd, 0) == pdTRUE) {
        applyCommand(cmd);
        // Free memory allocated for pointers in the command
        if (cmd.type == ShowCommandType::SET_SHOW || cmd.type == ShowCommandType::LOAD_PRESET) {
            free(cmd.show_name);
            free(cmd.params_json);
        }
    }
#endif
}

bool ShowController::queueLayoutChange(bool reverse, bool mirror, int16_t dead_leds) {
#ifdef ARDUINO
    if (commandQueue == nullptr) {
        return false;
    }

    ShowCommand cmd;
    cmd.type = ShowCommandType::SET_LAYOUT;
    cmd.layout_reverse = reverse;
    cmd.layout_mirror = mirror;
    cmd.layout_dead_leds = dead_leds;

    if (xQueueSend(commandQueue, &cmd, 0) == pdTRUE) {
        return true;
    }

    ESP_LOGW(TAG, "Layout command queue full!");
    return false;
#else
    return false;
#endif
}

bool ShowController::queuePresetLoad(const Config::Preset &preset) {
#ifdef ARDUINO
    if (commandQueue == nullptr) {
        return false;
    }

    ShowCommand cmd;
    cmd.type = ShowCommandType::LOAD_PRESET;

    // ShowCommand uses pointers; Preset stores C strings.
    cmd.show_name = strdup(preset.show_name);
    cmd.params_json = strdup(preset.params_json);

    cmd.layout_reverse = preset.layout_reverse;
    cmd.layout_mirror = preset.layout_mirror;
    cmd.layout_dead_leds = preset.layout_dead_leds;

    if (xQueueSend(commandQueue, &cmd, 0) == pdTRUE) {
        ESP_LOGI(TAG, "Queued preset load '%s'", preset.name);
        return true;
    }

    // If failed to queue, we must free the memory
    free(cmd.show_name);
    free(cmd.params_json);

    ESP_LOGW(TAG, "Preset load command queue full!");
    return false;
#else
    return false;
#endif
}

void ShowController::setStrip(std::unique_ptr<Strip::Strip> &&base) {
    baseStrip = std::move(base);

    if (baseStrip) {
        // Load and apply layout configuration
#ifdef ARDUINO
        ESP_LOGI(TAG, "Loading layout configuration...");
#endif
        Config::LayoutConfig layoutConfig = config.loadLayoutConfig();
        layout = std::make_unique<Strip::Layout>(*baseStrip, layoutConfig.reverse, layoutConfig.mirror,
                                                 layoutConfig.dead_leds);
#ifdef ARDUINO
        ESP_LOGI(TAG, "Layout initialized: reverse=%d, mirror=%d, dead_leds=%u",
                      layoutConfig.reverse, layoutConfig.mirror, layoutConfig.dead_leds);
#endif

        // Load and apply gamma configuration
        Config::DeviceConfig deviceConfig = config.loadDeviceConfig();
        auto basePtr = static_cast<Strip::Base*>(baseStrip.get());
        basePtr->setGammaMode(deviceConfig.gamma_mode);
#ifdef ARDUINO
        ESP_LOGI(TAG, "Gamma mode set to: %d", deviceConfig.gamma_mode);
#endif
    } else {
#ifdef ARDUINO
        ESP_LOGE(TAG, "Failed to initialize layout! base strip not set");
#endif
    }
}

void ShowController::clearStrip() {
#ifdef ARDUINO
    if (layout) {
        Strip::Color black = color(0, 0, 0);
        layout->fill(black);
        layout->show();

        ESP_LOGI(TAG, "Strip cleared");
    }
#endif
}

ShowController::~ShowController() {
#ifdef ARDUINO
    if (commandQueue != nullptr) {
        vQueueDelete(commandQueue);
        commandQueue = nullptr;
    }
#endif
}

const std::vector<ShowFactory::ShowInfo> &ShowController::listShows() const {
    return factory.listShows();
}

uint16_t ShowController::getCycleTime() const {
    return config.loadDeviceConfig().cycle_time;
}

void ShowController::executeShow(unsigned int iteration) const {
    if (layout && currentShow) {
        layout->setBrightness(brightness.load());
        currentShow->execute(*layout, iteration);
    }
}

void ShowController::show() const {
    if (layout) {
        layout->show();
    }
}

bool ShowController::isShowComplete() const {
    return currentShow && currentShow->isComplete();
}

void ShowController::updateStats(const ShowStats &newStats) {
    std::lock_guard<std::mutex> lock(stateMutex);
    stats = newStats;
}

ShowStats ShowController::getStats() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return stats;
}

std::string ShowController::getCurrentShowName() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return currentShowName;
}
