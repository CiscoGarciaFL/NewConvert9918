#pragma once

#include "newconvert9918/core/RgbImage.hpp"
#include "newconvert9918/core/TargetData.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace newconvert9918::core {

enum class MedianCutColorDepth : std::uint8_t {
    Rgb444,
    Rgb888,
};

enum class PopularityWeighting : std::uint8_t {
    Uniform,
    HorizontalCenter,
};

enum class PaletteSelectionError : std::uint8_t {
    None,
    InvalidColorCount,
    UnsupportedColorDepth,
    UnsupportedWeighting,
    PaletteRejected,
};

struct PaletteSelectionResult {
    PaletteSelectionError error{PaletteSelectionError::None};
    std::optional<Palette> palette;

    [[nodiscard]] explicit operator bool() const
    {
        return error == PaletteSelectionError::None && palette.has_value();
    }
};

// Rgb444 matches the original F18A palette path: channels are reduced to
// nibbles before partitioning, averaged with integer truncation, and expanded
// back to 8-bit values by duplicating the nibble.
[[nodiscard]] PaletteSelectionResult
selectMedianCutPalette(const RgbImage& source,
                       std::size_t desiredColorCount = 15,
                       MedianCutColorDepth colorDepth = MedianCutColorDepth::Rgb444);

// Popularity selection always uses the target's 12-bit RGB color space. The
// center weighting reproduces the original eight horizontal bands.
[[nodiscard]] PaletteSelectionResult
selectPopularPalette(const RgbImage& source,
                     std::size_t desiredColorCount = 15,
                     PopularityWeighting weighting = PopularityWeighting::HorizontalCenter);

} // namespace newconvert9918::core
