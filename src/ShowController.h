//
// Created by Claude Code on 03.01.26.
//
// ShowController - Thread-safe show management using FreeRTOS queue
//

#ifndef UNTITLED_SHOWCONTROLLER_H
#define UNTITLED_SHOWCONTROLLER_H

#include <memory>

#ifdef ARDUINO
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#endif

#include "show/Show.h"
#include "ShowFactory.h"
#include "Config.h"
#include "strip/Strip.h"
#include "strip/Layout.h"

/**
 * Show command types for queue communication
 */
enum class ShowCommandType {
    SET_SHOW,           // Change current show
    SET_BRIGHTNESS,     // Change brightness
    TOGGLE_AUTO_CYCLE,  // Enable/disable auto-cycling
    SET_LAYOUT          // Change strip layout
};

/**
 * Show command structure for queue
 */
struct ShowCommand {
    ShowCommandType type;
    char show_name[32];
    char params_json[256];  // JSON parameters for show
    uint8_t brightness_value;
    bool auto_cycle_enabled;
    bool layout_reverse;
    bool layout_mirror;
    uint16_t layout_dead_leds;
};

/**
 * ShowController
 * Thread-safe controller for managing LED shows across cores
 * - Core 1 (webserver) queues commands
 * - Core 0 (LED task) processes commands
 */
class ShowController {
private:
#ifdef ARDUINO
    QueueHandle_t commandQueue;
#endif

    ShowFactory &factory;
    Config::ConfigManager &config;

    std::unique_ptr<Show::Show> currentShow;
    char currentShowName[32];
    uint8_t brightness;
    bool autoCycle;

    // Pointer to layout for runtime reconfiguration
    std::unique_ptr<Strip::Strip>* layoutPtr;
    Strip::Strip* baseStrip;

    /**
     * Apply a command (called from LED task)
     */
    void applyCommand(const ShowCommand& cmd);

public:
    /**
     * ShowController constructor
     * @param factory Show factory for creating shows
     * @param config Configuration manager for persistence
     */
    ShowController(ShowFactory &factory, Config::ConfigManager &config);

    /**
     * Initialize the controller (call from setup)
     */
    void begin();

    /**
     * Queue a show change command (called from Core 1 - webserver)
     * @param showName Name of show to switch to
     * @param paramsJson JSON parameters (optional, defaults to "{}")
     * @return true if queued successfully
     */
    bool queueShowChange(const char* showName, const char* paramsJson = "{}");

    /**
     * Queue a brightness change command (called from Core 1 - webserver)
     * @param brightness New brightness value (0-255)
     * @return true if queued successfully
     */
    bool queueBrightnessChange(uint8_t brightness);

    /**
     * Queue auto-cycle toggle command (called from Core 1 - webserver)
     * @param enabled Enable/disable auto-cycling
     * @return true if queued successfully
     */
    bool queueAutoCycleToggle(bool enabled);

    /**
     * Queue layout change command (called from Core 1 - webserver)
     * @param reverse Reverse LED order
     * @param mirror Mirror LED pattern
     * @param dead_leds Number of dead LEDs at the end
     * @return true if queued successfully
     */
    bool queueLayoutChange(bool reverse, bool mirror, uint16_t dead_leds);

    /**
     * Set layout and base strip pointers for runtime reconfiguration
     * @param layout Pointer to the layout unique_ptr
     * @param base Pointer to the base strip
     */
    void setLayoutPointers(std::unique_ptr<Strip::Strip>* layout, Strip::Strip* base);

    /**
     * Process pending commands from queue (called from Core 0 - LED task)
     * Non-blocking - processes all pending commands
     */
    void processCommands();

    /**
     * Get current show pointer (called from Core 0 - LED task)
     * @return Current show instance
     */
    Show::Show* getCurrentShow();

    /**
     * Get current brightness
     * @return Brightness value (0-255)
     */
    uint8_t getBrightness() const { return brightness; }

    /**
     * Get current show name
     * @return Show name
     */
    const char* getCurrentShowName() const { return currentShowName; }

    /**
     * Get auto-cycle status
     * @return true if auto-cycling enabled
     */
    bool isAutoCycleEnabled() const { return autoCycle; }

    /**
     * Destructor
     */
    ~ShowController();
};

#endif //UNTITLED_SHOWCONTROLLER_H
