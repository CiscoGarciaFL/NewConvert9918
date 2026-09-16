#pragma once

#include "newconvert9918/core/ConversionTypes.hpp"
#include "newconvert9918/core/TargetData.hpp"

namespace newconvert9918::core {

// The original converter uses a fifteen-color working palette ordered as
// white, black, grey, then the twelve chromatic TMS9918A colors. The encoder
// remaps those working indexes to their hardware color codes.
[[nodiscard]] Palette defaultBitmap9918Palette();

// Converts an already scaled and preprocessed 256x192 image. Geometry,
// histogram stretching, and gamma correction remain separate core stages.
[[nodiscard]] ConversionResult
convertBitmap9918(const RgbImage& source,
                  const Palette& workingPalette,
                  const ConversionSettings& settings = {});

} // namespace newconvert9918::core
