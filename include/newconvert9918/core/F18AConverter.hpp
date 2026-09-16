#pragma once

#include "newconvert9918/core/ConversionTypes.hpp"

namespace newconvert9918::core {

// The caller selects the fifteen-color palette independently (normally with
// the RGB444 median-cut or popularity selector). Conversion rounds it to F18A
// precision before matching, previewing, and serializing it.
[[nodiscard]] ConversionResult
convertPalettedBitmapF18A(const RgbImage& source,
                          const Palette& selectedPalette,
                          const ConversionSettings& settings);

// Builds an independent RGB444 palette for every source scanline, quantizes
// each Graphics II row against its active palette, and emits 192 palette rows.
[[nodiscard]] ConversionResult
convertScanlinePaletteBitmapF18A(const RgbImage& source,
                                 const ConversionSettings& settings);

} // namespace newconvert9918::core
