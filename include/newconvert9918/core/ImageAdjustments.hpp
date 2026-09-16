#pragma once

#include "newconvert9918/core/ConversionSettings.hpp"
#include "newconvert9918/core/RgbImage.hpp"

#include <cstdint>
#include <optional>

namespace newconvert9918::core {

inline constexpr std::uint8_t histogramOutputMinimum = 32;
inline constexpr std::uint8_t histogramOutputMaximum = 224;

enum class ImageAdjustmentError : std::uint8_t {
    None,
    InvalidGamma,
    OutputImageRejected,
};

struct ImageAdjustmentResult {
    ImageAdjustmentError error{ImageAdjustmentError::None};
    std::optional<RgbImage> image;

    [[nodiscard]] explicit operator bool() const
    {
        return error == ImageAdjustmentError::None && image.has_value();
    }
};

// Applies histogram stretching first and gamma correction second, matching the
// ordering in Convert9918 1.9.1. The source image is never modified.
[[nodiscard]] ImageAdjustmentResult
adjustImage(const RgbImage& source,
            const ConversionSettings& settings,
            const ImageSizeLimits& limits = {});

} // namespace newconvert9918::core
