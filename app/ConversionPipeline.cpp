#include "ConversionPipeline.hpp"

#include "retrovdp/core/ConversionRegistry.hpp"
#include "retrovdp/core/Bitmap9918Converter.hpp"
#include "retrovdp/core/ImageAdjustments.hpp"
#include "retrovdp/core/TargetProfile.hpp"

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

    const auto& modeDescriptor = core::displayMode(request.settings.mode);
    const std::uint32_t modeWidth = modeDescriptor.geometry.width;
    const std::uint32_t modeHeight = modeDescriptor.geometry.height;

    core::ImageTransformResult transformed;
    if (options.sourceAlreadyFramed) {
        if (request.source->width() != modeWidth || request.source->height() != modeHeight) {
            return failedResult("preview-framed-source-size",
                                "The prepared source image does not match the selected mode.");
        }
        transformed.image = *request.source;
    } else {
        const core::ImageTransformOptions transformOptions{
            .targetWidth = options.powerPaintFraming ? 240U : modeWidth,
            .targetHeight = options.powerPaintFraming ? 160U : modeHeight,
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
                                "The source image could not be scaled to the selected mode geometry.");
        }
    }
    if (request.cancellation.isCancellationRequested()) return cancelledResult();

    if (options.powerPaintFraming && !options.sourceAlreadyFramed) {
        auto padded = core::RgbImage::createTightlyPacked(
            modeWidth, modeHeight, transformed.image->pixelFormat());
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
    return core::compileRegisteredConversion(
        *adjusted.image, request.settings, options.workingPalette,
        request.cancellation, std::move(options.progress));
}

} // namespace retrovdp::appsupport
