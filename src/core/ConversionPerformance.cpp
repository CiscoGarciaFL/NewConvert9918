#include "retrovdp/core/ConversionPerformance.hpp"

#include "retrovdp/core/ColorMath.hpp"
#include "retrovdp/core/Dithering.hpp"
#include "retrovdp/core/TargetData.hpp"
#include "retrovdp/core/TargetProfile.hpp"

namespace retrovdp::core {

ConversionMemoryEstimate estimateConversionMemory(const ConversionSettings& settings)
{
    const auto& mode = displayMode(settings.mode);
    const std::size_t pixelCount = static_cast<std::size_t>(mode.geometry.width)
        * mode.geometry.height;
    const std::size_t rgbImageBytes = pixelCount * 3U;

    ConversionMemoryEstimate estimate{
        .sourceImageBytes = rgbImageBytes,
        .previewImageBytes = rgbImageBytes,
        .indexedImageBytes = pixelCount,
    };
    const auto dithering = ditherConfiguration(settings.dither, settings.errorDistribution);
    const bool modeUsesErrorDiffusion = settings.mode != ConversionMode::Multicolor9918
        && settings.mode != ConversionMode::DualMulticolor9918;
    if (modeUsesErrorDiffusion && dithering && dithering->distributeError) {
        estimate.errorDiffusionBytes = pixelCount * sizeof(RgbSample);
    }
    for (const TargetTableLayout layout : expectedTargetTables(settings.mode)) {
        estimate.targetTableBytes += layout.byteSize;
    }
    if (settings.mode == ConversionMode::ScanlinePaletteBitmapF18A) {
        estimate.paletteBytes = 192U * 15U * sizeof(RgbColor);
    } else if (mode.palette.model == PaletteModel::ProgrammableRgb
               || mode.palette.model == PaletteModel::YjkWithPalette) {
        estimate.paletteBytes = static_cast<std::size_t>(mode.palette.workingColorCount)
            * sizeof(RgbColor);
    }
    return estimate;
}

} // namespace retrovdp::core
