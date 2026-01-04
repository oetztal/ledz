//
// Created by Claude Code on 03.01.26.
//
// ShowFactory - Manages show registration and creation
//

#ifndef UNTITLED_SHOWFACTORY_H
#define UNTITLED_SHOWFACTORY_H

#include <map>
#include <vector>
#include <functional>
#include <memory>

#ifdef ARDUINO
#include <Arduino.h>
#include <ArduinoJson.h>
#endif

#include "show/Show.h"

/**
 * ShowFactory
 * Factory pattern for creating LED shows by name
 * Supports show registration and parameter parsing
 */
class ShowFactory {
public:
    /**
     * Show constructor function type that takes JSON parameters
     */
    using ShowConstructor = std::function<std::unique_ptr<Show::Show>&&(const StaticJsonDocument<512>&)>;

    /**
     * Show metadata for listing available shows
     */
    struct ShowInfo {
        const char* name;
        const char* description;
    };

private:
    std::map<const char*, ShowConstructor, bool(*)(const char*, const char*)> showConstructors;
    std::vector<ShowInfo> showList;

    static bool strLess(const char* a, const char* b) {
        return strcmp(a, b) < 0;
    }

public:
    ShowFactory();

    /**
     * Register a show with the factory
     * @param name Show name (e.g., "Rainbow")
     * @param description Human-readable description
     * @param constructor Function that creates the show instance
     */
    void registerShow(const char* name, const char* description, ShowConstructor constructor);

    /**
     * Create a show by name
     * @param name Show name
     * @return Show instance (caller owns pointer) or nullptr if not found
     */
    std::unique_ptr<Show::Show> &&createShow(const char *name);

    /**
     * Create a show by name with JSON parameters
     * @param name Show name
     * @param paramsJson JSON string with parameters (e.g., {"r":255,"g":0,"b":0})
     * @return Show instance (caller owns pointer) or nullptr if not found
     */
    std::unique_ptr<Show::Show> &&createShow(const char *name, const char *paramsJson);

    /**
     * Get list of all registered shows
     * @return Vector of show names
     */
    const std::vector<ShowInfo>& listShows() const;

    /**
     * Check if a show is registered
     * @param name Show name
     * @return true if registered
     */
    bool hasShow(const char* name) const;
};

#endif //UNTITLED_SHOWFACTORY_H
