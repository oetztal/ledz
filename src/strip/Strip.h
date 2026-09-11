#ifndef LEDZ_STRIP_H
#define LEDZ_STRIP_H

#include <memory>

namespace Strip {
    typedef int16_t Pin;
    typedef int16_t PixelIndex;
    typedef uint32_t Color;
    typedef uint8_t ColorComponent;
    typedef uint8_t Brightness;

    class Strip {
    public:
        virtual ~Strip() = default;

        virtual void fill(Color color) = 0;

        virtual void setPixelColor(PixelIndex pixel_index, Color color);

        [[nodiscard]] virtual Color getPixelColor(PixelIndex pixel_index) const;

        [[nodiscard]] virtual PixelIndex length() const = 0;

        virtual void show() = 0;

        virtual void setBrightness(Brightness brightness) = 0;

        [[nodiscard]] virtual Brightness getBrightness() const;
    };
}

#endif //LEDZ_STRIP_H
