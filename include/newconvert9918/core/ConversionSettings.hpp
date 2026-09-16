#pragma once

#include <cstdint>

namespace newconvert9918::core {

enum class ConversionMode : std::uint8_t {
    Bitmap9918,
    GreyscaleBitmap9918,
    BlackAndWhiteBitmap9918,
    Multicolor9918,
    DualMulticolor9918,
    HalfMulticolor9918,
    BitmapColorOnly9918,
    PalettedBitmapF18A,
    ScanlinePaletteBitmapF18A,
};

enum class DitherMode : std::uint8_t {
    None,
    FloydSteinberg,
    Atkinson,
    Pattern,
    Diagonal,
    Ordered,
    OrderedWithError,
};

enum class OrderedDitherMapSize : std::uint8_t {
    TwoByTwo = 2,
    FourByFour = 4,
};

enum class ErrorAccumulationMode : std::uint8_t {
    Average,
    Accumulate,
};

struct ConversionSettings {
    ConversionMode mode{ConversionMode::Bitmap9918};
    DitherMode dither{DitherMode::Atkinson};
    OrderedDitherMapSize orderedDitherMapSize{OrderedDitherMapSize::TwoByTwo};
    int orderedDitherBrightness{0};
    ErrorAccumulationMode errorAccumulation{ErrorAccumulationMode::Accumulate};
    int targetWidth{256};
    int targetHeight{192};
    double gamma{1.0};
    double maximumColorShiftPercent{1.0};
    bool perceptualColorMatching{false};
    double perceptualRedWeight{0.30};
    double perceptualGreenWeight{0.52};
    double perceptualBlueWeight{0.18};
    double lumaEmphasis{1.2};
    bool stretchHistogram{false};
};

} // namespace newconvert9918::core
