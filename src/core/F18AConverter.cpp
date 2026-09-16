#include "newconvert9918/core/F18AConverter.hpp"

#include "newconvert9918/core/Bitmap9918Converter.hpp"
#include "newconvert9918/core/ColorMath.hpp"
#include "newconvert9918/core/Dithering.hpp"
#include "newconvert9918/core/PaletteSelection.hpp"
#include "newconvert9918/core/Validation.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace newconvert9918::core {
namespace {

constexpr std::uint32_t imageWidth = 256;
constexpr std::uint32_t imageHeight = 192;
constexpr std::size_t workingColorCount = 15;
constexpr std::size_t paletteTableSize = 32;
constexpr std::size_t bitmapTableSize = 6144;
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

std::uint8_t roundToF18A(std::uint8_t channel)
{
    return static_cast<std::uint8_t>((channel & 0xf0) | (channel >> 4));
}

Palette roundedPalette(const Palette& selected)
{
    std::vector<RgbColor> colors;
    colors.reserve(selected.size());
    for (const RgbColor color : selected.colors()) {
        colors.push_back({
            roundToF18A(color.red),
            roundToF18A(color.green),
            roundToF18A(color.blue),
        });
    }
    return std::move(*Palette::create(std::move(colors)));
}

std::uint8_t hardwareColor(std::uint8_t workingColor)
{
    if (workingColor == 0) {
        return 15;
    }
    if (workingColor == 2) {
        return 14;
    }
    return workingColor > 2 ? static_cast<std::uint8_t>(workingColor - 1)
                            : workingColor;
}

std::vector<std::uint8_t> encodePalette(const Palette& palette)
{
    std::vector<std::uint8_t> bytes(paletteTableSize);
    for (std::size_t index = 0; index < palette.size(); ++index) {
        const std::size_t hardwareIndex = hardwareColor(
            static_cast<std::uint8_t>(index));
        const RgbColor color = palette.at(index);
        bytes[hardwareIndex * 2] = static_cast<std::uint8_t>(color.red >> 4);
        bytes[hardwareIndex * 2 + 1] = static_cast<std::uint8_t>(
            (color.green & 0xf0) | (color.blue >> 4));
    }
    return bytes;
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
    for (std::size_t foreground = 0; foreground + 1 < palette.size();
         ++foreground) {
        for (std::size_t background = foreground + 1; background < palette.size();
             ++background) {
            for (int pattern = 0; pattern < 256; ++pattern) {
                RgbSample carried{};
                RgbSample farCarried{};
                double distance = 0.0;
                int mask = 0x80;
                for (std::size_t bit = 0; bit < desired.size(); ++bit) {
                    const std::size_t colorIndex = (pattern & mask) != 0
                        ? foreground
                        : background;
                    RgbSample candidate = desired[bit];
                    if (dithering.distributeError) {
                        candidate.red += carried.red / errorDivisor;
                        candidate.green += carried.green / errorDivisor;
                        candidate.blue += carried.blue / errorDivisor;
                    }
                    const RgbSample color{palette.at(colorIndex)};
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
                }
            }
        }
    }
    return best;
}

std::optional<Palette> selectScanlinePalette(const RgbImage& source,
                                              std::uint32_t y)
{
    auto line = RgbImage::createTightlyPacked(imageWidth, 1, PixelFormat::Rgb888);
    if (!line) {
        return std::nullopt;
    }
    std::span<std::uint8_t> destination = line->row(0);
    for (std::uint32_t x = 0; x < imageWidth; ++x) {
        const RgbSample sample = sourceSample(source, x, y);
        const std::size_t offset = static_cast<std::size_t>(x) * 3;
        destination[offset] = static_cast<std::uint8_t>(sample.red);
        destination[offset + 1] = static_cast<std::uint8_t>(sample.green);
        destination[offset + 2] = static_cast<std::uint8_t>(sample.blue);
    }
    PaletteSelectionResult selected = selectMedianCutPalette(
        *line, workingColorCount, MedianCutColorDepth::Rgb444);
    return selected ? std::move(selected.palette) : std::nullopt;
}

std::pair<std::vector<std::uint8_t>, std::vector<std::uint8_t>>
encodeBitmapTables(std::span<const std::uint8_t> indexed)
{
    std::vector<std::uint8_t> patterns(bitmapTableSize);
    std::vector<std::uint8_t> colors(bitmapTableSize);
    std::size_t output = 0;
    for (std::uint32_t cellY = 0; cellY < imageHeight; cellY += 8) {
        for (std::uint32_t cellX = 0; cellX < imageWidth; cellX += 8) {
            for (std::uint32_t row = 0; row < 8; ++row) {
                const std::size_t input = static_cast<std::size_t>(cellY + row)
                    * imageWidth + cellX;
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

std::optional<RgbImage> makeScanlinePreview(
    std::span<const std::uint8_t> indexed,
    std::span<const Palette> palettes)
{
    auto preview = RgbImage::createTightlyPacked(
        imageWidth, imageHeight, PixelFormat::Rgb888);
    if (!preview) {
        return std::nullopt;
    }
    for (std::uint32_t y = 0; y < imageHeight; ++y) {
        std::span<std::uint8_t> row = preview->row(y);
        for (std::uint32_t x = 0; x < imageWidth; ++x) {
            const RgbColor color = palettes[y].at(
                indexed[static_cast<std::size_t>(y) * imageWidth + x]);
            const std::size_t offset = static_cast<std::size_t>(x) * 3;
            row[offset] = color.red;
            row[offset + 1] = color.green;
            row[offset + 2] = color.blue;
        }
    }
    return preview;
}

std::vector<std::uint8_t> encodeScanlinePalettes(std::span<const Palette> palettes)
{
    std::vector<std::uint8_t> bytes(imageHeight * paletteTableSize);
    for (std::size_t y = 0; y < palettes.size(); ++y) {
        const std::vector<std::uint8_t> line = encodePalette(palettes[y]);
        std::copy(line.begin(), line.end(), bytes.begin() + y * paletteTableSize);
    }
    return bytes;
}

} // namespace

ConversionResult convertPalettedBitmapF18A(const RgbImage& source,
                                           const Palette& selectedPalette,
                                           const ConversionSettings& settings)
{
    if (settings.mode != ConversionMode::PalettedBitmapF18A) {
        return failure("paletted-bitmap-f18a-wrong-mode",
                       "The Paletted Bitmap F18A converter does not match the selected mode.");
    }
    if (!validate(settings).empty()) {
        return failure("paletted-bitmap-f18a-invalid-settings",
                       "Paletted Bitmap F18A conversion settings are invalid.");
    }
    if (settings.targetWidth != static_cast<int>(imageWidth)
        || settings.targetHeight != static_cast<int>(imageHeight)
        || source.width() != imageWidth || source.height() != imageHeight) {
        return failure("paletted-bitmap-f18a-invalid-dimensions",
                       "Paletted Bitmap F18A conversion requires a 256x192 source and target.");
    }
    if (selectedPalette.size() != workingColorCount) {
        return failure("paletted-bitmap-f18a-invalid-palette",
                       "Paletted Bitmap F18A conversion requires fifteen selected colors.");
    }

    const Palette palette = roundedPalette(selectedPalette);
    ConversionSettings bitmapSettings = settings;
    bitmapSettings.mode = ConversionMode::Bitmap9918;
    ConversionResult result = convertBitmap9918(source, palette, bitmapSettings);
    if (!result.succeeded() || !result.target) {
        return failure("paletted-bitmap-f18a-conversion-failed",
                       "Paletted Bitmap F18A Graphics II conversion failed.");
    }

    result.target->mode = ConversionMode::PalettedBitmapF18A;
    result.target->palette = palette;
    result.target->tables.push_back({TargetTableRole::Palette, encodePalette(palette)});
    return result;
}

ConversionResult convertScanlinePaletteBitmapF18A(
    const RgbImage& source,
    const ConversionSettings& settings)
{
    if (settings.mode != ConversionMode::ScanlinePaletteBitmapF18A) {
        return failure("scanline-palette-f18a-wrong-mode",
                       "The Scanline Palette Bitmap F18A converter does not match the selected mode.");
    }
    if (!validate(settings).empty()) {
        return failure("scanline-palette-f18a-invalid-settings",
                       "Scanline Palette Bitmap F18A conversion settings are invalid.");
    }
    if (settings.targetWidth != static_cast<int>(imageWidth)
        || settings.targetHeight != static_cast<int>(imageHeight)
        || source.width() != imageWidth || source.height() != imageHeight) {
        return failure("scanline-palette-f18a-invalid-dimensions",
                       "Scanline Palette Bitmap F18A requires a 256x192 source and target.");
    }
    const auto dithering = ditherConfiguration(settings.dither);
    if (!dithering) {
        return failure("scanline-palette-f18a-invalid-dither",
                       "Scanline Palette Bitmap F18A received an unsupported dither mode.");
    }

    std::vector<Palette> palettes;
    palettes.reserve(imageHeight);
    for (std::uint32_t y = 0; y < imageHeight; ++y) {
        auto palette = selectScanlinePalette(source, y);
        if (!palette) {
            return failure("scanline-palette-f18a-palette-selection",
                           "A scanline F18A palette could not be selected.");
        }
        palettes.push_back(std::move(*palette));
    }

    std::optional<ErrorDiffusionBuffer> errors;
    if (dithering->distributeError) {
        errors = ErrorDiffusionBuffer::create(imageWidth, imageHeight);
        if (!errors) {
            return failure("scanline-palette-f18a-error-buffer",
                           "The scanline palette error buffer could not be allocated.");
        }
    }
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
    std::vector<std::uint8_t> indexed(
        static_cast<std::size_t>(imageWidth) * imageHeight);
    for (std::uint32_t y = 0; y < imageHeight; ++y) {
        const Palette& palette = palettes[y];
        const double errorDivisor = settings.errorAccumulation
                == ErrorAccumulationMode::Average
                && y != 0
            ? 3.0
            : 1.0;
        for (std::uint32_t blockX = 0; blockX < imageWidth; blockX += 8) {
            std::array<RgbSample, 8> desired{};
            for (std::uint32_t bit = 0; bit < 8; ++bit) {
                const std::uint32_t x = blockX + bit;
                RgbSample sample = shiftTowardPalette(
                    sourceSample(source, x, y),
                    palette,
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
                desired, palette, settings, *dithering, errorDivisor);
            int mask = 0x80;
            RgbSample carried{};
            RgbSample farCarried{};
            for (std::uint32_t bit = 0; bit < 8; ++bit) {
                const std::uint32_t x = blockX + bit;
                const std::uint8_t colorIndex = (choice.pattern & mask) != 0
                    ? choice.foreground
                    : choice.background;
                indexed[static_cast<std::size_t>(y) * imageWidth + x] = colorIndex;
                if (errors) {
                    const RgbSample color{palette.at(colorIndex)};
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

    auto preview = makeScanlinePreview(indexed, palettes);
    if (!preview) {
        return failure("scanline-palette-f18a-preview-allocation",
                       "The Scanline Palette Bitmap F18A preview could not be allocated.");
    }
    auto [patterns, colors] = encodeBitmapTables(indexed);
    TargetMemoryImage target{
        .mode = ConversionMode::ScanlinePaletteBitmapF18A,
        .palette = std::nullopt,
        .tables = {
            {TargetTableRole::Pattern, std::move(patterns)},
            {TargetTableRole::Color, std::move(colors)},
            {TargetTableRole::ScanlinePalettes, encodeScanlinePalettes(palettes)},
        },
    };
    return {
        .status = ConversionStatus::Succeeded,
        .preview = std::move(preview),
        .diagnostics = {{
            DiagnosticSeverity::Information,
            "scanline-palette-deterministic-selection",
            "Scanline palettes use deterministic RGB444 median cut instead of the original stateful neighborhood merger.",
        }},
        .target = std::move(target),
    };
}

} // namespace newconvert9918::core
