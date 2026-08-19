#include <memory>

#include "Log.h"
#include "Network.h"
#include "Config.h"
#include "WebServerManager.h"
#include "ShowFactory.h"
#include "ShowController.h"
#include "strip/Base.h"
#include "task/LedShow.h"
#include "OTAUpdater.h"

static const char* TAG = "main";


TaskHandle_t networkTaskHandle = nullptr;

Config::ConfigManager config;
ShowFactory showFactory;
ShowController showController(showFactory, config);
Task::LedShow ledShow(showController);
Network network(config, showController);

void setup() {
    delay(1000);
    Serial.println("");
    // config.reset();
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
    } catch (const std::exception &e) {
        ESP_LOGE(TAG, "Error initializing LED strip: %s", e.what());
    } catch (...) {
        ESP_LOGE(TAG, "Unknown error initializing LED strip");
    }
#endif


    // Initialize show controller
    showController.begin();

    // Start tasks on their designated cores
    // LED task: Core 1 (isolated from WiFi)
    // Network task: Core 0 (same as WiFi stack)

    try {
        ledShow.startTask();
    } catch (const std::exception &e) {
        ESP_LOGE(TAG, "Error starting LED show task: %s", e.what());
    } catch (...) {
        ESP_LOGE(TAG, "Unknown error starting LED show task");
    }

    try {
        network.startTask();
    } catch (const std::exception &e) {
        ESP_LOGE(TAG, "Error starting network task: %s", e.what());
    } catch (...) {
        ESP_LOGE(TAG, "Unknown error starting network task");
    }
}

void loop() {
#ifdef ARDUINO
    config.checkRestart();
    vTaskDelay(250 / portTICK_PERIOD_MS);
#endif
}
