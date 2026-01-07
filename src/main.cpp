//
// Created by Andreas W. on 02.01.26.
//

#include <memory>

#include "Network.h"
#include "Config.h"
#include "WebServerManager.h"
#include "ShowFactory.h"
#include "ShowController.h"
#include "strip/Base.h"
#include "task/LedShow.h"


TaskHandle_t networkTaskHandle = nullptr;

Config::ConfigManager config;
ShowFactory showFactory;
ShowController showController(showFactory, config);
Task::LedShow ledShow(showController);
Network network(config, showController);

void setup() {
    delay(1000);
    Serial.println("Started");
    config.begin();

#ifdef ARDUINO
    // Load device configuration
    Config::DeviceConfig deviceConfig = config.loadDeviceConfig();
    uint16_t num_pixels = deviceConfig.num_pixels;
    uint8_t led_pin = deviceConfig.led_pin;
    Serial.printf("Initializing LED strip with %u pixels on pin %u\n", num_pixels, led_pin);

    // Initialize base strip with configured pin and number of pixels
    auto base = std::make_unique<Strip::Base>(led_pin, num_pixels);

    // Set layout pointers for runtime reconfiguration
    showController.setStrip(std::move(base));
#endif

    // Initialize show controller
    showController.begin();

    // Network will create the appropriate webserver instance based on mode
    ledShow.startTask();
    network.startTask();
}

void loop() {
#ifdef ARDUINO
    // Auto-cycling is now handled by ShowController and webserver
    // This loop is just for Arduino framework compatibility
    vTaskDelay(60000 / portTICK_PERIOD_MS);
#endif
}
