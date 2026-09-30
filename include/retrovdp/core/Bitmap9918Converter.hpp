#pragma once

#include "retrovdp/core/ConversionTypes.hpp"
#include "retrovdp/core/TargetData.hpp"

namespace retrovdp::core {

// The original converter uses a fifteen-color working palette ordered as
// white, black, grey, then the twelve chromatic TMS9918A colors. The encoder
// remaps those working indexes to their hardware color codes.
[[nodiscard]] Palette defaultBitmap9918Palette();

// The original greyscale mode converts every working-palette entry with
// Rec.709 luminance coefficients before running the Graphics II search.
[[nodiscard]] Palette greyscaleBitmap9918Palette(const Palette& colorPalette);

// Converts an already scaled and preprocessed 256x192 image. Geometry,
// histogram stretching, and gamma correction remain separate core stages.
[[nodiscard]] ConversionResult
convertBitmap9918(const RgbImage& source,
                  const Palette& workingPalette,
                  const ConversionSettings& settings = {},
                  CancellationToken cancellation = {},
                  ConversionProgressCallback progress = {});

[[nodiscard]] ConversionResult
convertGreyscaleBitmap9918(const RgbImage& source,
                           const Palette& workingPalette,
                           const ConversionSettings& settings,
                           CancellationToken cancellation = {},
                           ConversionProgressCallback progress = {});

[[nodiscard]] ConversionResult
convertBlackAndWhiteBitmap9918(const RgbImage& source,
                               const Palette& workingPalette,
                               const ConversionSettings& settings,
                               CancellationToken cancellation = {},
                               ConversionProgressCallback progress = {});

[[nodiscard]] ConversionResult
convertBitmapColorOnly9918(const RgbImage& source,
                           const Palette& workingPalette,
                           const ConversionSettings& settings,
                           CancellationToken cancellation = {},
                           ConversionProgressCallback progress = {});

} // namespace retrovdp::core
