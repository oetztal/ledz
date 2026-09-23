#include <memory>
#include <new>

#include "Log.h"
#include "Network.h"
#include "Config.h"
#include "WebServerManager.h"
#include "show/factory/ShowFactory.h"
#include "ShowController.h"
#include "strip/Base.h"
#include "task/LedShow.h"
#include "OTAUpdater.h"

static const char* const TAG = "main";

namespace {

    struct App {
        Config::ConfigManager config;
        Show::Factory::ShowFactory showFactory;
        ShowController showController{showFactory, config};
        Task::LedShow ledShow{showController};
        Network network{config, showController};
    };

    App& app() {
        static App instance;
        return instance;
    }

} // namespace

void setup() {
    auto& config = app().config;
    auto& showController = app().showController;
    auto& ledShow = app().ledShow;
    auto& network = app().network;

    delay(1000);
    Serial.println("");
    config.begin();
    OTAUpdater::setConfig(&config);

#ifdef ARDUINO
    // Load device configuration
    Config::DeviceConfig deviceConfig = config.loadDeviceConfig();
    uint16_t num_pixels = deviceConfig.num_pixels;
    uint8_t led_pin = deviceConfig.led_pin;
    ESP_LOGI(TAG, "Initializing LED strip with %u pixels on pin %u", num_pixels, led_pin);

    // Initialize base strip with configured pin and number of pixels
    try {
        auto base = std::make_unique<Strip::Base>(led_pin, num_pixels);

        // Set layout pointers for runtime reconfiguration
        showController.setStrip(std::move(base));
    } catch (const std::bad_alloc& e) {
        ESP_LOGE(TAG, "Out of memory initializing LED strip: %s", e.what());
    }
#endif

    // Initialize show controller
    showController.begin();

    // Start tasks on their designated cores
    // LED task: Core 1 (isolated from WiFi)
    // Network task: Core 0 (same as WiFi stack)

    // startTask() forwards to xTaskCreatePinnedToCore and does not throw.
    ledShow.startTask();
    network.startTask();
}

void loop() {
#ifdef ARDUINO
    app().config.checkRestart();
    vTaskDelay(250 / portTICK_PERIOD_MS);
#endif
}
