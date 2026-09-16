#include "newconvert9918/core/Bitmap9918Converter.hpp"

#include "newconvert9918/core/ColorMath.hpp"
#include "newconvert9918/core/Dithering.hpp"
#include "newconvert9918/core/Validation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace newconvert9918::core {
namespace {

constexpr std::uint32_t bitmapWidth = 256;
constexpr std::uint32_t bitmapHeight = 192;
constexpr std::size_t bitmapTableSize = 6144;
constexpr std::size_t standardWorkingColorCount = 15;
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

RgbSample greyscaleSourceSample(RgbSample source,
                                const ConversionSettings& settings)
{
    double luminanceValue = luminance(source);
    if (settings.perceptualColorMatching) {
        luminanceValue = source.red * settings.perceptualRedWeight
            + source.green * settings.perceptualGreenWeight
            + source.blue * settings.perceptualBlueWeight + 0.5;
    }
    luminanceValue = std::clamp(luminanceValue, 0.0, 255.0);
    const double grey = static_cast<double>(static_cast<int>(luminanceValue));
    return {grey, grey, grey};
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
                        double errorDivisor,
                        bool colorOnly)
{
    BlockChoice best;
    const double rightScale = static_cast<double>(dithering.kernel.right)
        / distributionDivisor;
    const double farRightScale = static_cast<double>(dithering.kernel.farRight)
        / distributionDivisor;

    for (std::size_t foreground = 0; foreground + 1 < palette.size();
         ++foreground) {
        const std::size_t backgroundStart = colorOnly ? 0 : foreground + 1;
        for (std::size_t background = backgroundStart;
             background < palette.size();
             ++background) {
            const int patternStart = colorOnly ? 0xf0 : 0;
            const int patternEnd = colorOnly ? 0xf1 : 256;
            for (int pattern = patternStart; pattern < patternEnd; ++pattern) {
                RgbSample carried{};
                RgbSample farCarried{};
                double distance = 0.0;
                int mask = 0x80;

                for (std::size_t bit = 0; bit < desired.size(); ++bit) {
                    const std::uint8_t colorIndex = static_cast<std::uint8_t>(
                        (pattern & mask) != 0 ? foreground : background);
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
                        static_cast<std::uint8_t>(foreground),
                        static_cast<std::uint8_t>(background),
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
encodeTables(std::span<const std::uint8_t> indexed,
             bool monochrome,
             bool colorOnly)
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

                if (monochrome) {
                    if (foreground != 1 || background == 1) {
                        pattern = static_cast<std::uint8_t>(~pattern);
                    }
                    foreground = 1;
                    background = 0;
                } else if (colorOnly) {
                    if (pattern == 0x00) {
                        foreground = background;
                    } else if (pattern == 0xff) {
                        background = foreground;
                    }
                    pattern = 0xf0;
                } else if (foregroundCount > backgroundCount) {
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

Palette greyscaleBitmap9918Palette(const Palette& colorPalette)
{
    std::vector<RgbColor> colors;
    colors.reserve(colorPalette.size());
    for (const RgbColor color : colorPalette.colors()) {
        const int luminanceValue = static_cast<int>(
            color.blue * 0.0722 + color.green * 0.7152 + color.red * 0.2126);
        const auto grey = static_cast<std::uint8_t>(luminanceValue);
        colors.push_back({grey, grey, grey});
    }
    return std::move(*Palette::create(std::move(colors)));
}

namespace {

ConversionResult convertBitmap9918Impl(const RgbImage& source,
                                       const Palette& workingPalette,
                                       const ConversionSettings& settings,
                                       ConversionMode requiredMode,
                                       bool greyscaleSource,
                                       bool monochrome,
                                       bool colorOnly,
                                       std::string_view diagnosticPrefix)
{
    const std::string codePrefix(diagnosticPrefix);
    if (settings.mode != requiredMode) {
        return failure(codePrefix + "-wrong-mode",
                       "The requested Bitmap 9918A converter does not match the selected mode.");
    }
    if (!validate(settings).empty()) {
        return failure(codePrefix + "-invalid-settings",
                       "Bitmap 9918A conversion settings are invalid.");
    }
    if (settings.targetWidth != static_cast<int>(bitmapWidth)
        || settings.targetHeight != static_cast<int>(bitmapHeight)
        || source.width() != bitmapWidth || source.height() != bitmapHeight) {
        return failure(codePrefix + "-invalid-dimensions",
                       "Bitmap 9918A conversion requires a 256x192 source and target.");
    }
    const std::size_t expectedPaletteSize = monochrome ? 2 : standardWorkingColorCount;
    if (workingPalette.size() != expectedPaletteSize) {
        return failure(codePrefix + "-invalid-palette",
                       "Bitmap 9918A conversion requires fifteen working colors.");
    }

    const auto dithering = ditherConfiguration(settings.dither);
    if (!dithering) {
        return failure(codePrefix + "-invalid-dither",
                       "Bitmap 9918A conversion received an unsupported dither mode.");
    }

    std::optional<ErrorDiffusionBuffer> errors;
    if (dithering->distributeError) {
        errors = ErrorDiffusionBuffer::create(bitmapWidth, bitmapHeight);
        if (!errors) {
            return failure(codePrefix + "-error-buffer",
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
                RgbSample input = sourceSample(source, x, y);
                if (greyscaleSource) {
                    input = greyscaleSourceSample(input, settings);
                }
                RgbSample sample = shiftTowardPalette(
                    input,
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
                desired,
                workingPalette,
                settings,
                *dithering,
                errorDivisor,
                colorOnly);
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
        return failure(codePrefix + "-preview-allocation",
                       "The Bitmap 9918A preview could not be allocated.");
    }

    auto [patterns, colors] = encodeTables(indexed, monochrome, colorOnly);
    std::vector<TargetMemoryTable> tables;
    tables.push_back({colorOnly ? TargetTableRole::FixedPattern
                               : TargetTableRole::Pattern,
                      std::move(patterns)});
    if (!monochrome) {
        tables.push_back({TargetTableRole::Color, std::move(colors)});
    }
    TargetMemoryImage target{
        .mode = requiredMode,
        .palette = std::nullopt,
        .tables = std::move(tables),
    };
    return {
        .status = ConversionStatus::Succeeded,
        .preview = std::move(preview),
        .diagnostics = {},
        .target = std::move(target),
    };
}

} // namespace

ConversionResult convertBitmap9918(const RgbImage& source,
                                   const Palette& workingPalette,
                                   const ConversionSettings& settings)
{
    return convertBitmap9918Impl(source,
                                 workingPalette,
                                 settings,
                                 ConversionMode::Bitmap9918,
                                 false,
                                 false,
                                 false,
                                 "bitmap9918");
}

ConversionResult convertGreyscaleBitmap9918(const RgbImage& source,
                                            const Palette& workingPalette,
                                            const ConversionSettings& settings)
{
    if (workingPalette.size() != standardWorkingColorCount) {
        return failure("greyscale-bitmap9918-invalid-palette",
                       "Greyscale Bitmap 9918A conversion requires fifteen working colors.");
    }
    return convertBitmap9918Impl(source,
                                 greyscaleBitmap9918Palette(workingPalette),
                                 settings,
                                 ConversionMode::GreyscaleBitmap9918,
                                 true,
                                 false,
                                 false,
                                 "greyscale-bitmap9918");
}

ConversionResult convertBlackAndWhiteBitmap9918(const RgbImage& source,
                                                const Palette& workingPalette,
                                                const ConversionSettings& settings)
{
    if (workingPalette.size() != standardWorkingColorCount) {
        return failure("black-white-bitmap9918-invalid-palette",
                       "Black-and-White Bitmap 9918A conversion requires fifteen working colors.");
    }
    auto monochromePalette = Palette::create({
        workingPalette.at(0),
        workingPalette.at(1),
    });
    return convertBitmap9918Impl(source,
                                 *monochromePalette,
                                 settings,
                                 ConversionMode::BlackAndWhiteBitmap9918,
                                 true,
                                 true,
                                 false,
                                 "black-white-bitmap9918");
}

ConversionResult convertBitmapColorOnly9918(const RgbImage& source,
                                            const Palette& workingPalette,
                                            const ConversionSettings& settings)
{
    return convertBitmap9918Impl(source,
                                 workingPalette,
                                 settings,
                                 ConversionMode::BitmapColorOnly9918,
                                 false,
                                 false,
                                 true,
                                 "bitmap-color-only9918");
}

} // namespace newconvert9918::core
