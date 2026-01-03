//
// Created by Claude Code on 03.01.26.
//

#include "ShowController.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

ShowController::ShowController(ShowFactory &factory, Config::ConfigManager &config)
    : factory(factory), config(config), brightness(128), autoCycle(true)
#ifdef ARDUINO
    , commandQueue(nullptr)
#endif
{
    currentShowName[0] = '\0';
}

void ShowController::begin() {
#ifdef ARDUINO
    // Create FreeRTOS queue (5 commands deep)
    commandQueue = xQueueCreate(5, sizeof(ShowCommand));

    if (commandQueue == nullptr) {
        Serial.println("ERROR: Failed to create show command queue!");
        return;
    }

    Serial.println("ShowController: Queue created successfully");
#endif

    // Load configuration
    Config::ShowConfig showConfig = config.loadShowConfig();
    Config::DeviceConfig deviceConfig = config.loadDeviceConfig();

    // Apply loaded configuration
    brightness = deviceConfig.brightness;
    autoCycle = showConfig.auto_cycle;

    // Create initial show
    const char* initialShowName = showConfig.current_show;
    if (strlen(initialShowName) > 0 && factory.hasShow(initialShowName)) {
        strncpy(currentShowName, initialShowName, sizeof(currentShowName) - 1);
    } else {
        strncpy(currentShowName, "Rainbow", sizeof(currentShowName) - 1);
    }
    currentShowName[sizeof(currentShowName) - 1] = '\0';

    currentShow.reset(factory.createShow(currentShowName));

    if (currentShow) {
#ifdef ARDUINO
        Serial.print("ShowController: Initial show loaded: ");
        Serial.println(currentShowName);
#endif
    } else {
#ifdef ARDUINO
        Serial.println("ERROR: Failed to create initial show!");
#endif
    }
}

bool ShowController::queueShowChange(const char* showName) {
#ifdef ARDUINO
    if (commandQueue == nullptr) {
        return false;
    }

    ShowCommand cmd;
    cmd.type = ShowCommandType::SET_SHOW;
    strncpy(cmd.show_name, showName, sizeof(cmd.show_name) - 1);
    cmd.show_name[sizeof(cmd.show_name) - 1] = '\0';

    // Try to send with no wait (non-blocking)
    if (xQueueSend(commandQueue, &cmd, 0) == pdTRUE) {
        return true;
    }

    Serial.println("WARNING: Show command queue full!");
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

    Serial.println("WARNING: Brightness command queue full!");
    return false;
#else
    return false;
#endif
}

bool ShowController::queueAutoCycleToggle(bool enabled) {
#ifdef ARDUINO
    if (commandQueue == nullptr) {
        return false;
    }

    ShowCommand cmd;
    cmd.type = ShowCommandType::TOGGLE_AUTO_CYCLE;
    cmd.auto_cycle_enabled = enabled;

    if (xQueueSend(commandQueue, &cmd, 0) == pdTRUE) {
        return true;
    }

    Serial.println("WARNING: Auto-cycle command queue full!");
    return false;
#else
    return false;
#endif
}

void ShowController::applyCommand(const ShowCommand& cmd) {
    switch (cmd.type) {
        case ShowCommandType::SET_SHOW: {
            // Create new show
            Show::Show* newShow = factory.createShow(cmd.show_name);
            if (newShow != nullptr) {
                currentShow.reset(newShow);
                strncpy(currentShowName, cmd.show_name, sizeof(currentShowName) - 1);
                currentShowName[sizeof(currentShowName) - 1] = '\0';

#ifdef ARDUINO
                Serial.print("ShowController: Switched to show: ");
                Serial.println(currentShowName);
#endif

                // Save to configuration
                Config::ShowConfig showConfig = config.loadShowConfig();
                strncpy(showConfig.current_show, currentShowName, sizeof(showConfig.current_show) - 1);
                config.saveShowConfig(showConfig);
            } else {
#ifdef ARDUINO
                Serial.print("ERROR: Failed to create show: ");
                Serial.println(cmd.show_name);
#endif
            }
            break;
        }

        case ShowCommandType::SET_BRIGHTNESS: {
            brightness = cmd.brightness_value;

#ifdef ARDUINO
            Serial.print("ShowController: Brightness set to: ");
            Serial.println(brightness);
#endif

            // Save to configuration
            Config::DeviceConfig deviceConfig = config.loadDeviceConfig();
            deviceConfig.brightness = brightness;
            config.saveDeviceConfig(deviceConfig);
            break;
        }

        case ShowCommandType::TOGGLE_AUTO_CYCLE: {
            autoCycle = cmd.auto_cycle_enabled;

#ifdef ARDUINO
            Serial.print("ShowController: Auto-cycle ");
            Serial.println(autoCycle ? "enabled" : "disabled");
#endif

            // Save to configuration
            Config::ShowConfig showConfig = config.loadShowConfig();
            showConfig.auto_cycle = autoCycle;
            config.saveShowConfig(showConfig);
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
    }
#endif
}

Show::Show* ShowController::getCurrentShow() {
    return currentShow.get();
}

ShowController::~ShowController() {
#ifdef ARDUINO
    if (commandQueue != nullptr) {
        vQueueDelete(commandQueue);
        commandQueue = nullptr;
    }
#endif
}
