#include "newconvert9918/core/Multicolor9918Converter.hpp"

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
#include <string>
#include <utility>
#include <vector>

namespace newconvert9918::core {
namespace {

constexpr std::uint32_t imageWidth = 256;
constexpr std::uint32_t imageHeight = 192;
constexpr std::uint32_t blockSize = 4;
constexpr std::size_t workingColorCount = 15;
constexpr std::size_t tableSize = 1536;
constexpr std::size_t bitmapTableSize = 6144;
constexpr std::size_t halfMulticolorTableSize = 2048;
constexpr double distributionDivisor = 16.0;

struct OverlayChoice {
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

std::uint8_t chooseBlockColor(const RgbImage& source,
                              std::uint32_t blockX,
                              std::uint32_t blockY,
                              const Palette& palette,
                              const ConversionSettings& settings)
{
    std::uint8_t bestColor = 0;
    double bestDistance = std::numeric_limits<double>::max();
    for (std::size_t colorIndex = 0; colorIndex < palette.size(); ++colorIndex) {
        const RgbSample candidate{palette.at(colorIndex)};
        double distance = 0.0;
        for (std::uint32_t y = 0; y < blockSize; ++y) {
            for (std::uint32_t x = 0; x < blockSize; ++x) {
                distance += colorDistanceSquared(
                    candidate,
                    shiftTowardPalette(
                        sourceSample(source, blockX + x, blockY + y),
                        palette,
                        settings.maximumColorShiftPercent),
                    settings);
            }
        }
        if (distance < bestDistance) {
            bestDistance = distance;
            bestColor = static_cast<std::uint8_t>(colorIndex);
        }
    }
    return bestColor;
}

std::uint8_t chooseHalfUnderlayColor(const RgbImage& source,
                                     std::uint32_t blockX,
                                     std::uint32_t blockY,
                                     const Palette& palette,
                                     const ConversionSettings& settings)
{
    std::uint8_t bestColor = 0;
    double bestDistance = std::numeric_limits<double>::max();
    const RgbColor black = palette.at(1);
    for (std::size_t colorIndex = 0; colorIndex < palette.size(); ++colorIndex) {
        const RgbColor base = palette.at(colorIndex);
        const RgbSample candidate{
            static_cast<double>((static_cast<int>(base.red) + black.red) / 2),
            static_cast<double>((static_cast<int>(base.green) + black.green) / 2),
            static_cast<double>((static_cast<int>(base.blue) + black.blue) / 2),
        };
        double distance = 0.0;
        for (std::uint32_t y = 0; y < blockSize; ++y) {
            for (std::uint32_t x = 0; x < blockSize; ++x) {
                const RgbSample shifted = shiftTowardPalette(
                    sourceSample(source, blockX + x, blockY + y),
                    palette,
                    settings.maximumColorShiftPercent);
                const RgbSample halved{
                    static_cast<double>(static_cast<int>(shifted.red) / 2),
                    static_cast<double>(static_cast<int>(shifted.green) / 2),
                    static_cast<double>(static_cast<int>(shifted.blue) / 2),
                };
                distance += colorDistanceSquared(candidate, halved, settings);
            }
        }
        if (distance < bestDistance) {
            bestDistance = distance;
            bestColor = static_cast<std::uint8_t>(colorIndex);
        }
    }
    return bestColor;
}

RgbColor mixedColor(const Palette& palette, std::uint8_t pair)
{
    const RgbColor first = palette.at(pair >> 4);
    const RgbColor second = palette.at(pair & 0x0f);
    return {
        static_cast<std::uint8_t>((static_cast<int>(first.red) + second.red) / 2),
        static_cast<std::uint8_t>((static_cast<int>(first.green) + second.green) / 2),
        static_cast<std::uint8_t>((static_cast<int>(first.blue) + second.blue) / 2),
    };
}

double flickerLuminance(RgbColor color)
{
    return color.red * 0.30 + color.green * 0.59 + color.blue * 0.11;
}

bool overlayPairAllowed(std::uint8_t leftUnderlay,
                        std::uint8_t rightUnderlay,
                        std::uint8_t foreground,
                        std::uint8_t background,
                        const Palette& palette,
                        int maximumDifferencePercent)
{
    const double ratio = maximumDifferencePercent * 2.55;
    const double squaredLimit = ratio * ratio;
    const std::array underlayLuminances{
        flickerLuminance(palette.at(leftUnderlay)),
        flickerLuminance(palette.at(rightUnderlay)),
    };
    const std::array overlayLuminances{
        flickerLuminance(palette.at(foreground)),
        flickerLuminance(palette.at(background)),
    };
    for (const double underlay : underlayLuminances) {
        for (const double overlay : overlayLuminances) {
            // This intentionally preserves the original one-sided squared-luma
            // comparison used by Half Multicolor.
            if (underlay * underlay - overlay * overlay > squaredLimit) {
                return false;
            }
        }
    }
    return true;
}

OverlayChoice chooseOverlay(
    const std::array<RgbSample, 8>& desired,
    const std::array<std::uint8_t, 8>& underlay,
    const Palette& palette,
    const ConversionSettings& settings,
    const DitherConfiguration& dithering,
    double errorDivisor)
{
    OverlayChoice best;
    const double rightScale = static_cast<double>(dithering.kernel.right)
        / distributionDivisor;
    const double farRightScale = static_cast<double>(dithering.kernel.farRight)
        / distributionDivisor;

    for (std::size_t foregroundIndex = 0;
         foregroundIndex + 1 < palette.size(); ++foregroundIndex) {
        const auto foreground = static_cast<std::uint8_t>(foregroundIndex);
        for (std::size_t backgroundIndex = foregroundIndex + 1;
             backgroundIndex < palette.size(); ++backgroundIndex) {
            const auto background = static_cast<std::uint8_t>(backgroundIndex);
            if (!overlayPairAllowed(underlay[0],
                                    underlay[4],
                                    foreground,
                                    background,
                                    palette,
                                    settings.maximumMulticolorDifferencePercent)) {
                continue;
            }
            for (int pattern = 0; pattern < 256; ++pattern) {
                RgbSample carried{};
                RgbSample farCarried{};
                double distance = 0.0;
                int mask = 0x80;
                for (std::size_t bit = 0; bit < desired.size(); ++bit) {
                    const std::uint8_t overlay = static_cast<std::uint8_t>(
                        (pattern & mask) != 0 ? foreground : background);
                    const RgbSample color{mixedColor(
                        palette,
                        static_cast<std::uint8_t>((overlay << 4) | underlay[bit]))};
                    RgbSample candidate = desired[bit];
                    if (dithering.distributeError) {
                        candidate.red += carried.red / errorDivisor;
                        candidate.green += carried.green / errorDivisor;
                        candidate.blue += carried.blue / errorDivisor;
                    }
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
                }
            }
        }
    }
    return best;
}

std::uint8_t chooseDualBlockColor(const RgbImage& source,
                                  std::uint32_t blockX,
                                  std::uint32_t blockY,
                                  const Palette& palette,
                                  const ConversionSettings& settings)
{
    std::uint8_t bestPair = 0;
    double bestDistance = std::numeric_limits<double>::max();
    const double maximumLuminanceDifference =
        static_cast<double>(settings.maximumMulticolorDifferencePercent) / 100.0;
    for (std::size_t first = 0; first < palette.size(); ++first) {
        for (std::size_t second = 0; second < palette.size(); ++second) {
            const double luminanceDifference = std::abs(
                luminance(RgbSample{palette.at(first)})
                - luminance(RgbSample{palette.at(second)}));
            if (luminanceDifference / 256.0 > maximumLuminanceDifference) {
                continue;
            }

            const std::uint8_t pair = static_cast<std::uint8_t>((first << 4) | second);
            const RgbSample candidate{mixedColor(palette, pair)};
            double distance = 0.0;
            for (std::uint32_t y = 0; y < blockSize; ++y) {
                for (std::uint32_t x = 0; x < blockSize; ++x) {
                    distance += colorDistanceSquared(
                        candidate,
                        shiftTowardPalette(
                            sourceSample(source, blockX + x, blockY + y),
                            palette,
                            settings.maximumColorShiftPercent),
                        settings);
                }
            }
            if (distance < bestDistance) {
                bestDistance = distance;
                bestPair = pair;
            }
        }
    }
    return bestPair;
}

std::optional<RgbImage> makePreview(std::span<const std::uint8_t> indexed,
                                    const Palette& palette)
{
    auto preview = RgbImage::createTightlyPacked(
        imageWidth, imageHeight, PixelFormat::Rgb888);
    if (!preview) {
        return std::nullopt;
    }
    for (std::uint32_t y = 0; y < imageHeight; ++y) {
        std::span<std::uint8_t> row = preview->row(y);
        for (std::uint32_t x = 0; x < imageWidth; ++x) {
            const RgbColor color = palette.at(
                indexed[static_cast<std::size_t>(y) * imageWidth + x]);
            const std::size_t offset = static_cast<std::size_t>(x) * 3;
            row[offset] = color.red;
            row[offset + 1] = color.green;
            row[offset + 2] = color.blue;
        }
    }
    return preview;
}

std::optional<RgbImage> makeDualPreview(std::span<const std::uint8_t> indexed,
                                        const Palette& palette)
{
    auto preview = RgbImage::createTightlyPacked(
        imageWidth, imageHeight, PixelFormat::Rgb888);
    if (!preview) {
        return std::nullopt;
    }
    for (std::uint32_t y = 0; y < imageHeight; ++y) {
        std::span<std::uint8_t> row = preview->row(y);
        for (std::uint32_t x = 0; x < imageWidth; ++x) {
            const RgbColor color = mixedColor(
                palette, indexed[static_cast<std::size_t>(y) * imageWidth + x]);
            const std::size_t offset = static_cast<std::size_t>(x) * 3;
            row[offset] = color.red;
            row[offset + 1] = color.green;
            row[offset + 2] = color.blue;
        }
    }
    return preview;
}

std::vector<std::uint8_t> encodeTable(std::span<const std::uint8_t> indexed)
{
    std::vector<std::uint8_t> table(tableSize);
    for (std::uint32_t y = 0; y < imageHeight; y += 8) {
        for (std::uint32_t x = 0; x < imageWidth; x += 8) {
            const std::size_t topLeft = static_cast<std::size_t>(y) * imageWidth + x;
            const std::uint8_t color1 = hardwareColor(indexed[topLeft]);
            const std::uint8_t color2 = hardwareColor(indexed[topLeft + 4]);
            const std::uint8_t color3 = hardwareColor(indexed[topLeft + imageWidth * 4]);
            const std::uint8_t color4 = hardwareColor(
                indexed[topLeft + imageWidth * 4 + 4]);

            const std::size_t characterGroup = y / 32;
            const std::size_t characterOffset = (y % 32) / 8;
            const std::size_t character = characterGroup * 32 + x / 8;
            const std::size_t address = character * 8 + characterOffset * 2;
            table[address] = static_cast<std::uint8_t>((color1 << 4) | color2);
            table[address + 1] = static_cast<std::uint8_t>((color3 << 4) | color4);
        }
    }
    return table;
}

std::vector<std::uint8_t> encodeDualTable(std::span<const std::uint8_t> indexed,
                                          bool firstFrame)
{
    std::vector<std::uint8_t> extracted(indexed.size());
    for (std::size_t index = 0; index < indexed.size(); ++index) {
        extracted[index] = firstFrame
            ? static_cast<std::uint8_t>(indexed[index] >> 4)
            : static_cast<std::uint8_t>(indexed[index] & 0x0f);
    }
    return encodeTable(extracted);
}

std::pair<std::vector<std::uint8_t>, std::vector<std::uint8_t>>
encodeHalfBitmapTables(std::span<const std::uint8_t> indexed)
{
    std::vector<std::uint8_t> patterns(bitmapTableSize);
    std::vector<std::uint8_t> colors(bitmapTableSize);
    std::size_t output = 0;
    for (std::uint32_t cellY = 0; cellY < imageHeight; cellY += 8) {
        for (std::uint32_t cellX = 0; cellX < imageWidth; cellX += 8) {
            for (std::uint32_t row = 0; row < 8; ++row) {
                const std::size_t input = static_cast<std::size_t>(cellY + row)
                    * imageWidth + cellX;
                std::uint8_t background = indexed[input] >> 4;
                std::uint8_t foreground = 1;
                int foregroundCount = 0;
                int backgroundCount = 0;
                std::uint8_t pattern = 0;
                std::uint8_t bit = 0x80;
                for (std::size_t pixel = 1; pixel < 8; ++pixel) {
                    bit >>= 1;
                    const std::uint8_t color = indexed[input + pixel] >> 4;
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

    std::rotate(patterns.begin() + 2048,
                patterns.begin() + 3840,
                patterns.begin() + 4096);
    std::rotate(colors.begin() + 2048,
                colors.begin() + 3840,
                colors.begin() + 4096);
    std::rotate(patterns.begin() + 4096,
                patterns.begin() + 5632,
                patterns.end());
    std::rotate(colors.begin() + 4096,
                colors.begin() + 5632,
                colors.end());
    return {std::move(patterns), std::move(colors)};
}

std::vector<std::uint8_t> encodeHalfMulticolorTable(
    std::span<const std::uint8_t> indexed)
{
    std::vector<std::uint8_t> table(halfMulticolorTableSize);
    for (std::uint32_t y = 0; y < imageHeight; y += 8) {
        for (std::uint32_t x = 0; x < imageWidth; x += 8) {
            const std::size_t input = static_cast<std::size_t>(y) * imageWidth + x;
            const std::uint8_t color1 = hardwareColor(indexed[input] & 0x0f);
            const std::uint8_t color2 = hardwareColor(indexed[input + 4] & 0x0f);
            const std::uint8_t color3 = hardwareColor(
                indexed[input + imageWidth * 4] & 0x0f);
            const std::uint8_t color4 = hardwareColor(
                indexed[input + imageWidth * 4 + 4] & 0x0f);

            const std::size_t bitmapGroup = (y / 8) / 8;
            const std::size_t multicolorGroup = (y / 8) % 4;
            std::size_t character = (y / 8) * 32 + x / 8;
            character = (character + 32 * bitmapGroup) & 0xff;
            const std::size_t address = character * 8 + 2 * multicolorGroup;
            table[address] = static_cast<std::uint8_t>((color1 << 4) | color2);
            table[address + 1] = static_cast<std::uint8_t>((color3 << 4) | color4);
        }
    }
    return table;
}

} // namespace

ConversionResult convertMulticolor9918(const RgbImage& source,
                                       const Palette& workingPalette,
                                       const ConversionSettings& settings)
{
    if (settings.mode != ConversionMode::Multicolor9918) {
        return failure("multicolor9918-wrong-mode",
                       "The Multicolor 9918 converter does not match the selected mode.");
    }
    if (!validate(settings).empty()) {
        return failure("multicolor9918-invalid-settings",
                       "Multicolor 9918 conversion settings are invalid.");
    }
    if (settings.targetWidth != static_cast<int>(imageWidth)
        || settings.targetHeight != static_cast<int>(imageHeight)
        || source.width() != imageWidth || source.height() != imageHeight) {
        return failure("multicolor9918-invalid-dimensions",
                       "Multicolor 9918 conversion requires a 256x192 source and target.");
    }
    if (workingPalette.size() != workingColorCount) {
        return failure("multicolor9918-invalid-palette",
                       "Multicolor 9918 conversion requires fifteen working colors.");
    }

    std::vector<std::uint8_t> indexed(
        static_cast<std::size_t>(imageWidth) * imageHeight);
    for (std::uint32_t y = 0; y < imageHeight; y += blockSize) {
        for (std::uint32_t x = 0; x < imageWidth; x += blockSize) {
            const std::uint8_t color = chooseBlockColor(
                source, x, y, workingPalette, settings);
            for (std::uint32_t subY = 0; subY < blockSize; ++subY) {
                for (std::uint32_t subX = 0; subX < blockSize; ++subX) {
                    indexed[static_cast<std::size_t>(y + subY) * imageWidth
                            + x + subX] = color;
                }
            }
        }
    }

    auto preview = makePreview(indexed, workingPalette);
    if (!preview) {
        return failure("multicolor9918-preview-allocation",
                       "The Multicolor 9918 preview could not be allocated.");
    }

    TargetMemoryImage target{
        .mode = ConversionMode::Multicolor9918,
        .palette = std::nullopt,
        .tables = {{TargetTableRole::Multicolor, encodeTable(indexed)}},
    };
    return {
        .status = ConversionStatus::Succeeded,
        .preview = std::move(preview),
        .diagnostics = {},
        .target = std::move(target),
    };
}

ConversionResult convertDualMulticolor9918(const RgbImage& source,
                                           const Palette& workingPalette,
                                           const ConversionSettings& settings)
{
    if (settings.mode != ConversionMode::DualMulticolor9918) {
        return failure("dual-multicolor9918-wrong-mode",
                       "The Dual Multicolor 9918 converter does not match the selected mode.");
    }
    if (!validate(settings).empty()) {
        return failure("dual-multicolor9918-invalid-settings",
                       "Dual Multicolor 9918 conversion settings are invalid.");
    }
    if (settings.targetWidth != static_cast<int>(imageWidth)
        || settings.targetHeight != static_cast<int>(imageHeight)
        || source.width() != imageWidth || source.height() != imageHeight) {
        return failure("dual-multicolor9918-invalid-dimensions",
                       "Dual Multicolor 9918 conversion requires a 256x192 source and target.");
    }
    if (workingPalette.size() != workingColorCount) {
        return failure("dual-multicolor9918-invalid-palette",
                       "Dual Multicolor 9918 conversion requires fifteen working colors.");
    }

    std::vector<std::uint8_t> indexed(
        static_cast<std::size_t>(imageWidth) * imageHeight);
    for (std::uint32_t y = 0; y < imageHeight; y += blockSize) {
        for (std::uint32_t x = 0; x < imageWidth; x += blockSize) {
            const std::uint8_t pair = chooseDualBlockColor(
                source, x, y, workingPalette, settings);
            for (std::uint32_t subY = 0; subY < blockSize; ++subY) {
                for (std::uint32_t subX = 0; subX < blockSize; ++subX) {
                    indexed[static_cast<std::size_t>(y + subY) * imageWidth
                            + x + subX] = pair;
                }
            }
        }
    }

    auto preview = makeDualPreview(indexed, workingPalette);
    if (!preview) {
        return failure("dual-multicolor9918-preview-allocation",
                       "The Dual Multicolor 9918 preview could not be allocated.");
    }

    TargetMemoryImage target{
        .mode = ConversionMode::DualMulticolor9918,
        .palette = std::nullopt,
        .tables = {
            {TargetTableRole::MulticolorFrame1, encodeDualTable(indexed, false)},
            {TargetTableRole::MulticolorFrame2, encodeDualTable(indexed, true)},
        },
    };
    return {
        .status = ConversionStatus::Succeeded,
        .preview = std::move(preview),
        .diagnostics = {},
        .target = std::move(target),
    };
}

ConversionResult convertHalfMulticolor9918(const RgbImage& source,
                                           const Palette& workingPalette,
                                           const ConversionSettings& settings)
{
    if (settings.mode != ConversionMode::HalfMulticolor9918) {
        return failure("half-multicolor9918-wrong-mode",
                       "The Half Multicolor 9918A converter does not match the selected mode.");
    }
    if (!validate(settings).empty()) {
        return failure("half-multicolor9918-invalid-settings",
                       "Half Multicolor 9918A conversion settings are invalid.");
    }
    if (settings.targetWidth != static_cast<int>(imageWidth)
        || settings.targetHeight != static_cast<int>(imageHeight)
        || source.width() != imageWidth || source.height() != imageHeight) {
        return failure("half-multicolor9918-invalid-dimensions",
                       "Half Multicolor 9918A conversion requires a 256x192 source and target.");
    }
    if (workingPalette.size() != workingColorCount) {
        return failure("half-multicolor9918-invalid-palette",
                       "Half Multicolor 9918A conversion requires fifteen working colors.");
    }
    const auto dithering = ditherConfiguration(settings.dither);
    if (!dithering) {
        return failure("half-multicolor9918-invalid-dither",
                       "Half Multicolor 9918A received an unsupported dither mode.");
    }

    std::vector<std::uint8_t> indexed(
        static_cast<std::size_t>(imageWidth) * imageHeight);
    for (std::uint32_t y = 0; y < imageHeight; y += blockSize) {
        for (std::uint32_t x = 0; x < imageWidth; x += blockSize) {
            const std::uint8_t underlay = chooseHalfUnderlayColor(
                source, x, y, workingPalette, settings);
            for (std::uint32_t subY = 0; subY < blockSize; ++subY) {
                for (std::uint32_t subX = 0; subX < blockSize; ++subX) {
                    indexed[static_cast<std::size_t>(y + subY) * imageWidth
                            + x + subX] = underlay;
                }
            }
        }
    }

    std::optional<ErrorDiffusionBuffer> errors;
    if (dithering->distributeError) {
        errors = ErrorDiffusionBuffer::create(imageWidth, imageHeight);
        if (!errors) {
            return failure("half-multicolor9918-error-buffer",
                           "The Half Multicolor 9918A error buffer could not be allocated.");
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

    for (std::uint32_t y = 0; y < imageHeight; ++y) {
        const double errorDivisor = settings.errorAccumulation
                == ErrorAccumulationMode::Average
                && y != 0
            ? 3.0
            : 1.0;
        for (std::uint32_t blockX = 0; blockX < imageWidth; blockX += 8) {
            std::array<RgbSample, 8> desired{};
            std::array<std::uint8_t, 8> underlay{};
            for (std::uint32_t bit = 0; bit < 8; ++bit) {
                const std::uint32_t x = blockX + bit;
                underlay[bit] = indexed[static_cast<std::size_t>(y) * imageWidth + x]
                    & 0x0f;
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

            const OverlayChoice choice = chooseOverlay(
                desired,
                underlay,
                workingPalette,
                settings,
                *dithering,
                errorDivisor);
            int mask = 0x80;
            RgbSample carried{};
            RgbSample farCarried{};
            for (std::uint32_t bit = 0; bit < 8; ++bit) {
                const std::uint32_t x = blockX + bit;
                const std::uint8_t overlay = static_cast<std::uint8_t>(
                    (choice.pattern & mask) != 0
                        ? choice.foreground
                        : choice.background);
                indexed[static_cast<std::size_t>(y) * imageWidth + x]
                    = static_cast<std::uint8_t>((overlay << 4) | underlay[bit]);

                if (errors) {
                    const RgbSample color{mixedColor(
                        workingPalette,
                        static_cast<std::uint8_t>((overlay << 4) | underlay[bit]))};
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

    auto preview = makeDualPreview(indexed, workingPalette);
    if (!preview) {
        return failure("half-multicolor9918-preview-allocation",
                       "The Half Multicolor 9918A preview could not be allocated.");
    }
    auto [patterns, colors] = encodeHalfBitmapTables(indexed);
    TargetMemoryImage target{
        .mode = ConversionMode::HalfMulticolor9918,
        .palette = std::nullopt,
        .tables = {
            {TargetTableRole::Pattern, std::move(patterns)},
            {TargetTableRole::Color, std::move(colors)},
            {TargetTableRole::Multicolor, encodeHalfMulticolorTable(indexed)},
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
