//
// Created by Andreas W. on 19.09.26.
//

#include "show/ColorRanges.h"
#include "strip/Strip.h"

#include "ColorRangesFactory.h"
#include "color.h"


namespace Show::Factory {
    static void extract_colors_from_array(std::vector<Strip::Color> &colors,
                                                       const JsonArrayConst &colorArray) {
        auto r = colorArray[0].as<uint8_t>();
        auto g = colorArray[1].as<uint8_t>();
        auto b = colorArray[2].as<uint8_t>();
        colors.push_back(color(r, g, b));
    }

    static void extract_colors_from_json_array(std::vector<Strip::Color> &colors,
                                                            const JsonArrayConst &colorsArray) {
        for (JsonVariantConst colorVariant: colorsArray) {
            auto colorArray = colorVariant.as<JsonArrayConst>();
            if (!colorArray.isNull() && colorArray.size() >= 3) {
                extract_colors_from_array(colors, colorArray);
            }
        }
    }

    std::unique_ptr<Show> ColorRangesFactory::createSolid(const JsonDocument &doc) {
        std::vector<Strip::Color> colors;
        std::vector<float> ranges;

        // Parse colors array (supports 1 or more colors)
        // JsonArrayConst, not JsonArray: doc is a const reference, so its
        // subscripts are JsonVariantConst and only convert to the const views.
        if (!doc["colors"].isNull()) {
            auto colorsArray = doc["colors"].as<JsonArrayConst>();
            if (!colorsArray.isNull() && colorsArray.size() > 0) {
                extract_colors_from_json_array(colors, colorsArray);
            }
        }

        // Parse ranges array (optional)
        if (!doc["ranges"].isNull()) {
            auto rangesArray = doc["ranges"].as<JsonArrayConst>();
            if (!rangesArray.isNull() && rangesArray.size() > 0) {
                for (JsonVariantConst rangeVariant: rangesArray) {
                    ranges.push_back(rangeVariant.as<float>());
                }
            }
        }

        // Parse gradient flag (optional, default false)
        bool gradient = doc["gradient"] | false;

        // If no colors parsed, use default warm white
        if (colors.empty()) {
            colors.push_back(color(255, 250, 230)); // Warm white
        }

        return std::make_unique<ColorRanges>(colors, ranges, gradient);
    }
}
