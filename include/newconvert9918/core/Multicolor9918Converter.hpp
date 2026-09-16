#pragma once

#include "newconvert9918/core/ConversionTypes.hpp"

namespace newconvert9918::core {

// Converts a prepared 256x192 RGB image into the TMS9918A's 64x48 logical
// multicolor grid and its 1536-byte pattern-generator table.
[[nodiscard]] ConversionResult
convertMulticolor9918(const RgbImage& source,
                      const Palette& workingPalette,
                      const ConversionSettings& settings);

} // namespace newconvert9918::core
