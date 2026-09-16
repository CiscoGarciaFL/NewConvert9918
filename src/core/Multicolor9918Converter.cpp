#include "newconvert9918/core/Multicolor9918Converter.hpp"

#include "newconvert9918/core/ColorMath.hpp"
#include "newconvert9918/core/Validation.hpp"

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
                    sourceSample(source, blockX + x, blockY + y),
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

} // namespace newconvert9918::core
