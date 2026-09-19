// ShowFactory - Manages show registration and creation
//

#ifndef LEDZ_SHOW_FACTORY_SHOWFACTORY_H
#define LEDZ_SHOW_FACTORY_SHOWFACTORY_H

#include <map>
#include <vector>
#include <functional>
#include <memory>
#include <string>
#include <ArduinoJson.h>

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "show/Show.h"
#include "Config.h"
#include "ColorRangesFactory.h"

namespace Show::Factory {

/**
 * ShowFactory
 * Factory pattern for creating LED shows by name
 * Supports show registration and parameter parsing
 */
class ShowFactory {
    ColorRangesFactory colorRangesFactory;

public:
    /**
     * Show constructor function type that takes JSON parameters
     */
    using ShowConstructor = std::function<std::unique_ptr<Show>(const JsonDocument &)>;

    /**
     * Show metadata for listing available shows
     */
    struct ShowInfo {
        std::string name;
        std::string description;
    };

private:
    std::map<std::string, ShowConstructor> showConstructors;
    std::vector<ShowInfo> showList;

public:
    ShowFactory(ColorRangesFactory color_ranges_factory);
    ShowFactory() : ShowFactory(ColorRangesFactory()) {}

    // disable copy constructor
    ShowFactory(const ShowFactory &) = delete;

    /**
     * Register a show with the factory
     * @param name Show name (e.g., "Rainbow")
     * @param description Human-readable description
     * @param constructor Function that creates the show instance
     */
    void registerShow(const std::string &name, const std::string &description, ShowConstructor &&constructor);

    /**
     * Create a show by name
     * @param name Show name
     * @return Show instance (caller owns pointer) or nullptr if not found
     */
    std::unique_ptr<Show> createShow(const std::string &name);

    /**
     * Create a show by name with JSON parameters
     * @param name Show name
     * @param paramsJson JSON string with parameters (e.g., {"r":255,"g":0,"b":0})
     * @return Show instance (caller owns pointer) or nullptr if not found
     */
    std::unique_ptr<Show> createShow(const std::string &name, const std::string &paramsJson);

    /**
     * Get list of all registered shows
     * @return Vector of show names
     */
    [[nodiscard]] const std::vector<ShowInfo> &listShows() const;

    /**
     * Check if a show is registered
     * @param name Show name
     * @return true if registered
     */
    bool hasShow(const std::string &name) const;
};

} // namespace Show::Factory

#endif //LEDZ_SHOW_FACTORY_SHOWFACTORY_H
