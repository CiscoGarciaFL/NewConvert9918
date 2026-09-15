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

struct ConversionSettings {
    ConversionMode mode{ConversionMode::Bitmap9918};
    DitherMode dither{DitherMode::FloydSteinberg};
    int targetWidth{256};
    int targetHeight{192};
    double gamma{1.0};
    double maximumColorShiftPercent{1.0};
    bool perceptualColorMatching{false};
    bool stretchHistogram{false};
};

} // namespace newconvert9918::core

