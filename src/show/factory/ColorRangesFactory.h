#ifndef LEDZ_SHOW_FACTORY_COLORRANGESFACTORY_H
#define LEDZ_SHOW_FACTORY_COLORRANGESFACTORY_H

#include <ArduinoJson.h>


namespace Show::Factory {
    class ColorRangesFactory {
    public:
        static std::unique_ptr<Show> createSolid(const JsonDocument &doc);
    };
}


#endif //LEDZ_SHOW_FACTORY_COLORRANGESFACTORY_H
