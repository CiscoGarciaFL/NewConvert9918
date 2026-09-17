#include "newconvert9918/core/ConversionPerformance.hpp"

#include "newconvert9918/core/ColorMath.hpp"
#include "newconvert9918/core/Dithering.hpp"
#include "newconvert9918/core/TargetData.hpp"

namespace newconvert9918::core {

ConversionMemoryEstimate estimateConversionMemory(const ConversionSettings& settings)
{
    constexpr std::size_t pixelCount = 256U * 192U;
    constexpr std::size_t rgbImageBytes = pixelCount * 3U;

    ConversionMemoryEstimate estimate{
        .sourceImageBytes = rgbImageBytes,
        .previewImageBytes = rgbImageBytes,
        .indexedImageBytes = pixelCount,
    };
    const auto dithering = ditherConfiguration(settings.dither);
    const bool modeUsesErrorDiffusion = settings.mode != ConversionMode::Multicolor9918
        && settings.mode != ConversionMode::DualMulticolor9918;
    if (modeUsesErrorDiffusion && dithering && dithering->distributeError) {
        estimate.errorDiffusionBytes = pixelCount * sizeof(RgbSample);
    }
    for (const TargetTableLayout layout : expectedTargetTables(settings.mode)) {
        estimate.targetTableBytes += layout.byteSize;
    }
    if (settings.mode == ConversionMode::PalettedBitmapF18A) {
        estimate.paletteBytes = 15U * sizeof(RgbColor);
    } else if (settings.mode == ConversionMode::ScanlinePaletteBitmapF18A) {
        estimate.paletteBytes = 192U * 15U * sizeof(RgbColor);
    }
    return estimate;
}

} // namespace newconvert9918::core
