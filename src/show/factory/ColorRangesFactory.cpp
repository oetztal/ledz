#include "show/ColorRanges.h"
#include "strip/Strip.h"

#include "ColorRangesFactory.h"
#include "../../support/Color.h"


namespace Show::Factory {
    static void extract_colors_from_array(std::vector<Strip::Color> &colors,
                                          const JsonArrayConst &colorArray) {
        auto r = colorArray[0].as<uint8_t>();
        auto g = colorArray[1].as<uint8_t>();
        auto b = colorArray[2].as<uint8_t>();
        colors.push_back(Support::Color::from_rgb(r, g, b));
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

    static void parse_colors(const JsonDocument &doc, std::vector<Strip::Color> &colors) {
        auto colorsArray = doc["colors"].as<JsonArrayConst>();
        if (!colorsArray.isNull() && colorsArray.size() > 0) {
            extract_colors_from_json_array(colors, colorsArray);
        }
    }

    static void parse_ranges(const JsonDocument &doc, std::vector<float> &ranges) {
        auto rangesArray = doc["ranges"].as<JsonArrayConst>();
        if (!rangesArray.isNull() && rangesArray.size() > 0) {
            for (JsonVariantConst rangeVariant: rangesArray) {
                ranges.push_back(rangeVariant.as<float>());
            }
        }
    }

    static void set_default_if_empty(std::vector<Strip::Color> &colors) {
        if (colors.empty()) {
            colors.push_back(Support::Color::from_rgb(255, 250, 230)); // Warm white
        }
    }

    std::unique_ptr<Show> ColorRangesFactory::createSolid(const JsonDocument &doc) {
        std::vector<Strip::Color> colors;
        std::vector<float> ranges;

        if (!doc["colors"].isNull()) {
            parse_colors(doc, colors);
        }

        if (!doc["ranges"].isNull()) {
            parse_ranges(doc, ranges);
        }

        bool gradient = doc["gradient"] | false;

        set_default_if_empty(colors);

        return std::make_unique<ColorRanges>(colors, ranges, gradient);
    }
}
