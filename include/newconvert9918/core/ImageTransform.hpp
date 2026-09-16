#pragma once

#include "newconvert9918/core/RgbImage.hpp"

#include <cstdint>
#include <optional>

namespace newconvert9918::core {

enum class ScalingFilter : std::uint8_t {
    Box,
    Gaussian,
    Hamming,
    Blackman,
    Bilinear,
    None,
};

enum class ImageFillMode : std::uint8_t {
    Fit,
    CropStart,
    CropCenter,
    CropEnd,
};

struct ImageTransformOptions {
    std::uint32_t targetWidth{256};
    std::uint32_t targetHeight{192};
    ScalingFilter filter{ScalingFilter::Bilinear};
    ImageFillMode fillMode{ImageFillMode::Fit};
    int horizontalOffset{};
    int verticalOffset{};
    std::uint8_t backgroundRed{};
    std::uint8_t backgroundGreen{};
    std::uint8_t backgroundBlue{};
    std::uint8_t backgroundAlpha{255};
};

struct ImageTransformPlan {
    std::uint32_t scaledWidth{};
    std::uint32_t scaledHeight{};
    std::uint32_t cropX{};
    std::uint32_t cropY{};
    std::uint32_t destinationX{};
    std::uint32_t destinationY{};
    std::uint32_t copyWidth{};
    std::uint32_t copyHeight{};
};

enum class ImageTransformError : std::uint8_t {
    None,
    ZeroTargetDimension,
    UnsupportedFilter,
    UnsupportedFillMode,
    ScaledImageLimitExceeded,
    OutputImageRejected,
};

struct ImageTransformResult {
    ImageTransformError error{ImageTransformError::None};
    ImageTransformPlan plan;
    std::optional<RgbImage> image;

    [[nodiscard]] explicit operator bool() const
    {
        return error == ImageTransformError::None && image.has_value();
    }
};

[[nodiscard]] std::optional<ImageTransformPlan>
planImageTransform(const RgbImage& source,
                   const ImageTransformOptions& options,
                   const ImageSizeLimits& limits = {},
                   ImageTransformError* error = nullptr);

[[nodiscard]] ImageTransformResult
transformImage(const RgbImage& source,
               const ImageTransformOptions& options,
               const ImageSizeLimits& limits = {});

} // namespace newconvert9918::core
