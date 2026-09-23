#ifndef LEDZ_STRIP_H
#define LEDZ_STRIP_H

#include <memory>

namespace Strip {
    using Pin = int16_t;
    using PixelIndex = int16_t;
    using Color = uint32_t;
    using ColorComponent = uint8_t;
    using Brightness = uint8_t;

    class Strip {
    public:
        virtual ~Strip() = default;

        virtual void fill(Color color) = 0;

        virtual void setPixelColor(PixelIndex pixel_index, Color color) = 0;

        [[nodiscard]] virtual Color getPixelColor(PixelIndex pixel_index) const = 0;

        [[nodiscard]] virtual PixelIndex length() const = 0;

        virtual void show() = 0;

        virtual void setBrightness(Brightness brightness) = 0;

        [[nodiscard]] virtual Brightness getBrightness() const = 0;
    };
}

#endif // LEDZ_STRIP_H
