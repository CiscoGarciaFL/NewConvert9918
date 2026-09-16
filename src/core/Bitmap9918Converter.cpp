#include "newconvert9918/core/Bitmap9918Converter.hpp"

#include "newconvert9918/core/ColorMath.hpp"
#include "newconvert9918/core/Dithering.hpp"
#include "newconvert9918/core/Validation.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace newconvert9918::core {
namespace {

constexpr std::uint32_t bitmapWidth = 256;
constexpr std::uint32_t bitmapHeight = 192;
constexpr std::size_t bitmapTableSize = 6144;
constexpr std::size_t workingColorCount = 15;
constexpr double distributionDivisor = 16.0;

struct BlockChoice {
    std::uint8_t foreground{};
    std::uint8_t background{};
    std::uint8_t pattern{};
    double distance{std::numeric_limits<double>::max()};
};

ConversionResult failure(std::string code, std::string message)
{
    return {
        .status = ConversionStatus::Failed,
        .preview = std::nullopt,
        .diagnostics = {{DiagnosticSeverity::Error, std::move(code), std::move(message)}},
        .target = std::nullopt,
    };
}

RgbSample paletteSample(const Palette& palette, std::size_t index)
{
    return RgbSample{palette.at(index)};
}

RgbSample sourceSample(const RgbImage& source, std::uint32_t x, std::uint32_t y)
{
    const std::span<const std::uint8_t> row = source.row(y);
    const std::size_t offset = static_cast<std::size_t>(x)
        * bytesPerPixel(source.pixelFormat());
    return {
        static_cast<double>(row[offset]),
        static_cast<double>(row[offset + 1]),
        static_cast<double>(row[offset + 2]),
    };
}

double rgbDistanceSquared(RgbSample left, RgbColor right)
{
    const double red = left.red - right.red;
    const double green = left.green - right.green;
    const double blue = left.blue - right.blue;
    return red * red + green * green + blue * blue;
}

double shiftedChannel(double source, std::uint8_t destination, double scale)
{
    const double movement = (static_cast<double>(destination) - source) * scale;
    return source + static_cast<int>(movement < 0.0 ? movement - 0.5 : movement + 0.5);
}

RgbSample shiftTowardPalette(RgbSample source,
                             const Palette& palette,
                             double percent)
{
    if (percent <= 0.0) {
        return source;
    }

    std::size_t closest = 0;
    double closestDistance = std::numeric_limits<double>::max();
    for (std::size_t index = 0; index < palette.size(); ++index) {
        const double distance = rgbDistanceSquared(source, palette.at(index));
        if (distance < closestDistance) {
            closestDistance = distance;
            closest = index;
        }
    }

    const RgbColor destination = palette.at(closest);
    const double scale = percent / 100.0;
    return {
        shiftedChannel(source.red, destination.red, scale),
        shiftedChannel(source.green, destination.green, scale),
        shiftedChannel(source.blue, destination.blue, scale),
    };
}

bool bypassIncomingDither(RgbSample sample)
{
    return (sample.red <= 8.0 && sample.green <= 8.0 && sample.blue <= 8.0)
        || (sample.red >= 248.0 && sample.green >= 248.0 && sample.blue >= 248.0);
}

BlockChoice chooseBlock(const std::array<RgbSample, 8>& desired,
                        const Palette& palette,
                        const ConversionSettings& settings,
                        const DitherConfiguration& dithering,
                        double errorDivisor)
{
    BlockChoice best;
    const double rightScale = static_cast<double>(dithering.kernel.right)
        / distributionDivisor;
    const double farRightScale = static_cast<double>(dithering.kernel.farRight)
        / distributionDivisor;

    for (std::uint8_t foreground = 0; foreground < workingColorCount - 1;
         ++foreground) {
        for (std::uint8_t background = foreground + 1;
             background < workingColorCount;
             ++background) {
            for (int pattern = 0; pattern < 256; ++pattern) {
                RgbSample carried{};
                RgbSample farCarried{};
                double distance = 0.0;
                int mask = 0x80;

                for (std::size_t bit = 0; bit < desired.size(); ++bit) {
                    const std::uint8_t colorIndex = (pattern & mask) != 0
                        ? foreground
                        : background;
                    RgbSample candidate = desired[bit];
                    if (dithering.distributeError) {
                        candidate.red += carried.red / errorDivisor;
                        candidate.green += carried.green / errorDivisor;
                        candidate.blue += carried.blue / errorDivisor;
                    }

                    const RgbSample color = paletteSample(palette, colorIndex);
                    distance += colorDistanceSquared(candidate, color, settings);
                    if (distance >= best.distance) {
                        break;
                    }

                    if (dithering.distributeError) {
                        const RgbSample error{
                            candidate.red - color.red,
                            candidate.green - color.green,
                            candidate.blue - color.blue,
                        };
                        carried = {
                            error.red * rightScale + farCarried.red,
                            error.green * rightScale + farCarried.green,
                            error.blue * rightScale + farCarried.blue,
                        };
                        farCarried = {
                            error.red * farRightScale,
                            error.green * farRightScale,
                            error.blue * farRightScale,
                        };
                    }
                    mask >>= 1;
                }

                if (distance < best.distance) {
                    best = {
                        foreground,
                        background,
                        static_cast<std::uint8_t>(pattern),
                        distance,
                    };
                    if (distance == 0.0) {
                        return best;
                    }
                }
            }
        }
    }
    return best;
}

std::uint8_t hardwareColor(std::uint8_t workingColor)
{
    if (workingColor == 0) {
        return 15;
    }
    if (workingColor == 2) {
        return 14;
    }
    return workingColor > 2 ? workingColor - 1 : workingColor;
}

std::pair<std::vector<std::uint8_t>, std::vector<std::uint8_t>>
encodeTables(std::span<const std::uint8_t> indexed)
{
    std::vector<std::uint8_t> patterns(bitmapTableSize);
    std::vector<std::uint8_t> colors(bitmapTableSize);
    std::size_t output = 0;

    for (std::uint32_t cellY = 0; cellY < bitmapHeight; cellY += 8) {
        for (std::uint32_t cellX = 0; cellX < bitmapWidth; cellX += 8) {
            for (std::uint32_t row = 0; row < 8; ++row) {
                const std::size_t input = static_cast<std::size_t>(cellY + row)
                    * bitmapWidth + cellX;
                std::uint8_t background = indexed[input];
                std::uint8_t foreground = 1;
                int foregroundCount = 0;
                int backgroundCount = 0;
                std::uint8_t pattern = 0;
                std::uint8_t bit = 0x80;

                for (std::size_t pixel = 1; pixel < 8; ++pixel) {
                    bit >>= 1;
                    const std::uint8_t color = indexed[input + pixel];
                    if (color != background) {
                        foreground = color;
                        pattern = static_cast<std::uint8_t>(pattern | bit);
                        ++foregroundCount;
                    } else {
                        ++backgroundCount;
                    }
                }

                if (foregroundCount > backgroundCount) {
                    std::swap(foreground, background);
                    pattern = static_cast<std::uint8_t>(~pattern);
                }

                patterns[output] = pattern;
                colors[output] = static_cast<std::uint8_t>(
                    (hardwareColor(foreground) << 4) | hardwareColor(background));
                ++output;
            }
        }
    }
    return {std::move(patterns), std::move(colors)};
}

std::optional<RgbImage> makePreview(std::span<const std::uint8_t> indexed,
                                    const Palette& palette)
{
    auto preview = RgbImage::createTightlyPacked(
        bitmapWidth, bitmapHeight, PixelFormat::Rgb888);
    if (!preview) {
        return std::nullopt;
    }

    for (std::uint32_t y = 0; y < bitmapHeight; ++y) {
        std::span<std::uint8_t> row = preview->row(y);
        for (std::uint32_t x = 0; x < bitmapWidth; ++x) {
            const RgbColor color = palette.at(
                indexed[static_cast<std::size_t>(y) * bitmapWidth + x]);
            const std::size_t offset = static_cast<std::size_t>(x) * 3;
            row[offset] = color.red;
            row[offset + 1] = color.green;
            row[offset + 2] = color.blue;
        }
    }
    return preview;
}

} // namespace

Palette defaultBitmap9918Palette()
{
    auto palette = Palette::create({
        {248, 248, 248},
        {0, 0, 0},
        {200, 200, 200},
        {32, 200, 64},
        {88, 216, 120},
        {80, 80, 232},
        {120, 112, 248},
        {208, 80, 72},
        {64, 232, 240},
        {248, 80, 80},
        {248, 120, 120},
        {208, 192, 80},
        {224, 200, 128},
        {32, 176, 56},
        {200, 88, 184},
    });
    return std::move(*palette);
}

ConversionResult convertBitmap9918(const RgbImage& source,
                                   const Palette& workingPalette,
                                   const ConversionSettings& settings)
{
    if (settings.mode != ConversionMode::Bitmap9918) {
        return failure("bitmap9918-wrong-mode",
                       "Bitmap 9918A conversion requires Bitmap9918 mode.");
    }
    if (!validate(settings).empty()) {
        return failure("bitmap9918-invalid-settings",
                       "Bitmap 9918A conversion settings are invalid.");
    }
    if (settings.targetWidth != static_cast<int>(bitmapWidth)
        || settings.targetHeight != static_cast<int>(bitmapHeight)
        || source.width() != bitmapWidth || source.height() != bitmapHeight) {
        return failure("bitmap9918-invalid-dimensions",
                       "Bitmap 9918A conversion requires a 256x192 source and target.");
    }
    if (workingPalette.size() != workingColorCount) {
        return failure("bitmap9918-invalid-palette",
                       "Bitmap 9918A conversion requires fifteen working colors.");
    }

    const auto dithering = ditherConfiguration(settings.dither);
    if (!dithering) {
        return failure("bitmap9918-invalid-dither",
                       "Bitmap 9918A conversion received an unsupported dither mode.");
    }

    std::optional<ErrorDiffusionBuffer> errors;
    if (dithering->distributeError) {
        errors = ErrorDiffusionBuffer::create(bitmapWidth, bitmapHeight);
        if (!errors) {
            return failure("bitmap9918-error-buffer",
                           "The Bitmap 9918A error buffer could not be allocated.");
        }
    }

    std::vector<std::uint8_t> indexed(
        static_cast<std::size_t>(bitmapWidth) * bitmapHeight);
    const ErrorDistributionKernel verticalKernel{
        dithering->kernel.downLeft,
        dithering->kernel.down,
        dithering->kernel.downRight,
        0,
        0,
        dithering->kernel.downTwo,
    };
    const ErrorDistributionKernel horizontalKernel{
        0,
        0,
        0,
        dithering->kernel.right,
        dithering->kernel.farRight,
        0,
    };

    for (std::uint32_t y = 0; y < bitmapHeight; ++y) {
        const double errorDivisor = settings.errorAccumulation
                == ErrorAccumulationMode::Average
                && y != 0
            ? 3.0
            : 1.0;

        for (std::uint32_t blockX = 0; blockX < bitmapWidth; blockX += 8) {
            std::array<RgbSample, 8> desired{};
            for (std::uint32_t bit = 0; bit < 8; ++bit) {
                const std::uint32_t x = blockX + bit;
                RgbSample sample = shiftTowardPalette(
                    sourceSample(source, x, y),
                    workingPalette,
                    settings.maximumColorShiftPercent);
                if (!bypassIncomingDither(sample)) {
                    if (dithering->ordered) {
                        sample = *applyOrderedDither(
                            sample,
                            x,
                            y,
                            settings.orderedDitherMapSize,
                            settings.orderedDitherBrightness);
                    }
                    if (errors) {
                        sample = errors->adjustedSample(
                            sample, x, y, settings.errorAccumulation);
                    }
                }
                desired[bit] = sample;
            }

            const BlockChoice choice = chooseBlock(
                desired, workingPalette, settings, *dithering, errorDivisor);
            int mask = 0x80;
            RgbSample carried{};
            RgbSample farCarried{};
            for (std::uint32_t bit = 0; bit < 8; ++bit) {
                const std::uint32_t x = blockX + bit;
                const std::uint8_t colorIndex = (choice.pattern & mask) != 0
                    ? choice.foreground
                    : choice.background;
                indexed[static_cast<std::size_t>(y) * bitmapWidth + x] = colorIndex;

                if (errors) {
                    const RgbSample color = paletteSample(workingPalette, colorIndex);
                    const RgbSample verticalError{
                        desired[bit].red - color.red,
                        desired[bit].green - color.green,
                        desired[bit].blue - color.blue,
                    };
                    errors->distribute(x, y, verticalError, verticalKernel);

                    const RgbSample horizontalSample{
                        desired[bit].red + carried.red / errorDivisor,
                        desired[bit].green + carried.green / errorDivisor,
                        desired[bit].blue + carried.blue / errorDivisor,
                    };
                    const RgbSample horizontalError{
                        horizontalSample.red - color.red,
                        horizontalSample.green - color.green,
                        horizontalSample.blue - color.blue,
                    };
                    errors->distribute(x, y, horizontalError, horizontalKernel);

                    const double rightScale = static_cast<double>(
                        dithering->kernel.right) / distributionDivisor;
                    const double farRightScale = static_cast<double>(
                        dithering->kernel.farRight) / distributionDivisor;
                    carried = {
                        horizontalError.red * rightScale + farCarried.red,
                        horizontalError.green * rightScale + farCarried.green,
                        horizontalError.blue * rightScale + farCarried.blue,
                    };
                    farCarried = {
                        horizontalError.red * farRightScale,
                        horizontalError.green * farRightScale,
                        horizontalError.blue * farRightScale,
                    };
                }
                mask >>= 1;
            }
        }
    }

    auto preview = makePreview(indexed, workingPalette);
    if (!preview) {
        return failure("bitmap9918-preview-allocation",
                       "The Bitmap 9918A preview could not be allocated.");
    }

    auto [patterns, colors] = encodeTables(indexed);
    TargetMemoryImage target{
        .mode = ConversionMode::Bitmap9918,
        .palette = std::nullopt,
        .tables = {
            {TargetTableRole::Pattern, std::move(patterns)},
            {TargetTableRole::Color, std::move(colors)},
        },
    };
    return {
        .status = ConversionStatus::Succeeded,
        .preview = std::move(preview),
        .diagnostics = {},
        .target = std::move(target),
    };
}

} // namespace newconvert9918::core
