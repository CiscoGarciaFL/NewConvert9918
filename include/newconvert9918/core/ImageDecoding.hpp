#pragma once

#include "newconvert9918/core/RgbImage.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace newconvert9918::core {

enum class ImageDecodeError : std::uint8_t {
    None,
    InvalidHeader,
    UnsupportedVariant,
    TruncatedData,
    MalformedData,
    AllocationLimitExceeded,
};

struct ImageDecodeResult {
    std::optional<RgbImage> image;
    ImageDecodeError error{ImageDecodeError::None};
    std::string message;

    [[nodiscard]] explicit operator bool() const { return image.has_value(); }
};

// Decodes ZSoft PCX without relying on a platform image plug-in. Supported
// inputs are 1-bit 1-4 plane images and 8-bit indexed, RGB, or RGBA images.
[[nodiscard]] ImageDecodeResult decodePcx(
    std::span<const std::uint8_t> bytes,
    const ImageSizeLimits& limits = {});

// Decodes a TMS9918A Graphics II table pair. paletteData may be empty, a
// 32-byte F18A palette, or a 6144-byte per-scanline palette. multicolorData may
// be empty or a 2048-byte Half Multicolor underlay table.
[[nodiscard]] ImageDecodeResult decodeGraphics2(
    std::span<const std::uint8_t> patternData,
    std::span<const std::uint8_t> colorData,
    std::span<const std::uint8_t> paletteData = {},
    std::span<const std::uint8_t> multicolorData = {},
    const ImageSizeLimits& limits = {});

[[nodiscard]] ImageDecodeResult decodeMsxScreen2(
    std::span<const std::uint8_t> bytes,
    const ImageSizeLimits& limits = {});
[[nodiscard]] ImageDecodeResult decodeColecoCvPaint(
    std::span<const std::uint8_t> bytes,
    const ImageSizeLimits& limits = {});
[[nodiscard]] ImageDecodeResult decodeAdamPowerPaint(
    std::span<const std::uint8_t> bytes,
    const ImageSizeLimits& limits = {});
[[nodiscard]] ImageDecodeResult decodeAdamHgr(
    std::span<const std::uint8_t> bytes,
    const ImageSizeLimits& limits = {});

} // namespace newconvert9918::core
