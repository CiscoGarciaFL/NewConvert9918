#include "newconvert9918/core/F18AConverter.hpp"

#include "newconvert9918/core/Bitmap9918Converter.hpp"
#include "newconvert9918/core/Validation.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace newconvert9918::core {
namespace {

constexpr std::uint32_t imageWidth = 256;
constexpr std::uint32_t imageHeight = 192;
constexpr std::size_t workingColorCount = 15;
constexpr std::size_t paletteTableSize = 32;

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

} // namespace newconvert9918::core
