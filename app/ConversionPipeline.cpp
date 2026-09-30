#include "ConversionPipeline.hpp"

#include "retrovdp/core/Bitmap9918Converter.hpp"
#include "retrovdp/core/F18AConverter.hpp"
#include "retrovdp/core/ImageAdjustments.hpp"
#include "retrovdp/core/Multicolor9918Converter.hpp"
#include "retrovdp/core/PaletteSelection.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <utility>

namespace retrovdp::appsupport {
namespace {

core::ConversionResult failedResult(std::string code, std::string message)
{
    core::ConversionResult result;
    result.status = core::ConversionStatus::Failed;
    result.diagnostics.push_back(
        {core::DiagnosticSeverity::Error, std::move(code), std::move(message)});
    return result;
}

core::ConversionResult cancelledResult()
{
    core::ConversionResult result;
    result.status = core::ConversionStatus::Cancelled;
    return result;
}

} // namespace

std::vector<core::RgbColor> defaultWorkingPalette()
{
    const auto palette = core::defaultBitmap9918Palette();
    return {palette.colors().begin(), palette.colors().end()};
}

core::ConversionResult runConversion(const core::ConversionRequest& request,
                                     ConversionPipelineOptions options)
{
    if (!request.source) {
        return failedResult("preview-missing-source", "No source image is loaded.");
    }
    if (request.cancellation.isCancellationRequested()) return cancelledResult();

    core::ImageTransformResult transformed;
    if (options.sourceAlreadyFramed) {
        if (request.source->width() != 256U || request.source->height() != 192U) {
            return failedResult("preview-framed-source-size",
                                "The prepared source image must be 256x192.");
        }
        transformed.image = *request.source;
    } else {
        const core::ImageTransformOptions transformOptions{
            .targetWidth = options.powerPaintFraming ? 240U : 256U,
            .targetHeight = options.powerPaintFraming ? 160U : 192U,
            .filter = options.scalingFilter,
            .fillMode = options.fillMode,
            .horizontalOffset = options.horizontalOffset,
            .verticalOffset = options.verticalOffset,
            .backgroundRed = options.backgroundColor.red,
            .backgroundGreen = options.backgroundColor.green,
            .backgroundBlue = options.backgroundColor.blue,
        };
        transformed = core::transformImage(*request.source, transformOptions);
        if (!transformed) {
            return failedResult("preview-transform-failed",
                                "The source image could not be scaled to 256x192.");
        }
    }
    if (request.cancellation.isCancellationRequested()) return cancelledResult();

    if (options.powerPaintFraming && !options.sourceAlreadyFramed) {
        auto padded = core::RgbImage::createTightlyPacked(
            256, 192, transformed.image->pixelFormat());
        if (!padded) {
            return failedResult("preview-powerpaint-padding-failed",
                                "The 240x160 PowerPaint frame could not be padded.");
        }
        std::fill(padded->bytes().begin(), padded->bytes().end(), 0);
        if (padded->pixelFormat() == core::PixelFormat::Rgba8888) {
            for (std::size_t offset = 3; offset < padded->bytes().size(); offset += 4) {
                padded->bytes()[offset] = 255;
            }
        }
        for (std::uint32_t y = 0; y < 160; ++y) {
            const auto sourceRow = transformed.image->row(y);
            auto destinationRow = padded->row(y);
            std::copy_n(sourceRow.begin(), transformed.image->minimumRowBytes(),
                        destinationRow.begin());
        }
        transformed.image = std::move(padded);
    }

    auto adjusted = core::adjustImage(*transformed.image, request.settings);
    if (!adjusted) {
        return failedResult("preview-adjustment-failed",
                            "The selected histogram or gamma adjustment failed.");
    }
    if (request.cancellation.isCancellationRequested()) return cancelledResult();

    if (options.workingPalette.empty()) {
        options.workingPalette = defaultWorkingPalette();
    }
    auto colorPaletteValue = core::Palette::create(std::move(options.workingPalette));
    if (!colorPaletteValue || colorPaletteValue->size() != 15U) {
        return failedResult("preview-working-palette-invalid",
                            "The working palette must contain fifteen colors.");
    }
    const core::Palette& colorPalette = *colorPaletteValue;
    switch (request.settings.mode) {
    case core::ConversionMode::Bitmap9918:
        return core::convertBitmap9918(
            *adjusted.image, colorPalette, request.settings, request.cancellation,
            options.progress);
    case core::ConversionMode::GreyscaleBitmap9918:
        return core::convertGreyscaleBitmap9918(
            *adjusted.image,
            core::greyscaleBitmap9918Palette(colorPalette),
            request.settings,
            request.cancellation,
            options.progress);
    case core::ConversionMode::BlackAndWhiteBitmap9918:
        return core::convertBlackAndWhiteBitmap9918(
            *adjusted.image, colorPalette, request.settings, request.cancellation,
            options.progress);
    case core::ConversionMode::Multicolor9918:
        return core::convertMulticolor9918(
            *adjusted.image, colorPalette, request.settings, request.cancellation,
            options.progress);
    case core::ConversionMode::DualMulticolor9918:
        return core::convertDualMulticolor9918(
            *adjusted.image, colorPalette, request.settings, request.cancellation,
            options.progress);
    case core::ConversionMode::HalfMulticolor9918:
        return core::convertHalfMulticolor9918(
            *adjusted.image, colorPalette, request.settings, request.cancellation,
            options.progress);
    case core::ConversionMode::BitmapColorOnly9918:
        return core::convertBitmapColorOnly9918(
            *adjusted.image, colorPalette, request.settings, request.cancellation,
            options.progress);
    case core::ConversionMode::PalettedBitmapF18A: {
        auto selected = request.settings.paletteSelection
                == core::PaletteSelectionMode::Popularity
            ? core::selectPopularPalette(
                  *adjusted.image, 15, core::PopularityWeighting::HorizontalCenter)
            : core::selectMedianCutPalette(
                  *adjusted.image, 15, core::MedianCutColorDepth::Rgb444);
        if (!selected) {
            return failedResult("preview-palette-failed",
                                "A 15-color F18A palette could not be selected.");
        }
        return core::convertPalettedBitmapF18A(
            *adjusted.image, *selected.palette, request.settings, request.cancellation,
            options.progress);
    }
    case core::ConversionMode::ScanlinePaletteBitmapF18A:
        return core::convertScanlinePaletteBitmapF18A(
            *adjusted.image, request.settings, request.cancellation, options.progress);
    }
    return failedResult("preview-unsupported-mode", "The conversion mode is unsupported.");
}

} // namespace retrovdp::appsupport
