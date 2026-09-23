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

    class ShowFactory {
    public:
        /**
         * Show constructor function type that takes JSON parameters
         */
        using ShowConstructor = std::function<std::unique_ptr<Show>(const JsonDocument&)>;

        /**
         * Show metadata for listing available shows
         */
        struct ShowInfo {
            std::string name;
            std::string description;
        };

    private:
        std::map<std::string, ShowConstructor, std::less<>> showConstructors;
        std::vector<ShowInfo> showList;

    public:
        ShowFactory();

        // disable copy constructor and copy assignment
        ShowFactory(const ShowFactory&) = delete;
        ShowFactory& operator=(const ShowFactory&) = delete;

        /**
         * Register a show with the factory
         * @param name Show name (e.g., "Rainbow")
         * @param description Human-readable description
         * @param constructor Function that creates the show instance
         */
        void registerShow(const std::string& name, const std::string& description, ShowConstructor&& constructor);

        /**
         * Create a show by name
         * @param name Show name
         * @return Show instance (caller owns pointer) or nullptr if not found
         */
        std::unique_ptr<Show> createShow(const std::string& name);

        /**
         * Create a show by name with JSON parameters
         * @param name Show name
         * @param paramsJson JSON string with parameters (e.g., {"r":255,"g":0,"b":0})
         * @return Show instance (caller owns pointer) or nullptr if not found
         */
        std::unique_ptr<Show> createShow(const std::string& name, const std::string& paramsJson);

        /**
         * Merge the JSON default parameters for a show (from the generated header)
         * with the user-supplied paramsJson. The default is parsed first; the user
         * payload is overlaid on top via ArduinoJson 7's deep-merge so explicit user
         * keys override the default and omitted user keys fall back to it. Returns
         * the merged JSON as a string. If the show is unknown, the user payload is
         * returned as-is.
         *
         * Exposed for the native parity test in test/test_show_factory/ so the test
         * can observe the merge without having to peek inside the factory's
         * internal JsonDocument.
         */
        std::string mergeDefaultParams(const std::string& name, const std::string& paramsJson) const;

        /**
         * Get list of all registered shows
         * @return Vector of show names
         */
        [[nodiscard]] const std::vector<ShowInfo>& listShows() const;

        /**
         * Check if a show is registered
         * @param name Show name
         * @return true if registered
         */
        bool hasShow(const std::string& name) const;
    };

} // namespace Show::Factory

#endif // LEDZ_SHOW_FACTORY_SHOWFACTORY_H
