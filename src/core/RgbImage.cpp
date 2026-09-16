#include "newconvert9918/core/RgbImage.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace newconvert9918::core {
namespace {

void setError(ImageLayoutError* destination, ImageLayoutError error)
{
    if (destination != nullptr) {
        *destination = error;
    }
}

} // namespace

ImageLayoutValidation validateImageLayout(const ImageLayout& layout,
                                          const ImageSizeLimits& limits)
{
    ImageLayoutValidation result;
    if (layout.width == 0) {
        result.error = ImageLayoutError::ZeroWidth;
        return result;
    }
    if (layout.height == 0) {
        result.error = ImageLayoutError::ZeroHeight;
        return result;
    }

    const std::size_t pixelSize = bytesPerPixel(layout.pixelFormat);
    if (pixelSize == 0) {
        result.error = ImageLayoutError::UnsupportedPixelFormat;
        return result;
    }
    if (layout.width > limits.maximumWidth || layout.height > limits.maximumHeight) {
        result.error = ImageLayoutError::DimensionLimitExceeded;
        return result;
    }

    const auto width = static_cast<std::size_t>(layout.width);
    const auto height = static_cast<std::size_t>(layout.height);
    if (width > std::numeric_limits<std::size_t>::max() / height
        || width * height > limits.maximumPixels) {
        result.error = ImageLayoutError::PixelLimitExceeded;
        return result;
    }
    if (width > std::numeric_limits<std::size_t>::max() / pixelSize) {
        result.error = ImageLayoutError::ByteSizeOverflow;
        return result;
    }

    result.minimumRowBytes = width * pixelSize;
    if (layout.rowStride < result.minimumRowBytes) {
        result.error = ImageLayoutError::RowStrideTooSmall;
        return result;
    }
    if (layout.rowStride > std::numeric_limits<std::size_t>::max() / height) {
        result.error = ImageLayoutError::ByteSizeOverflow;
        return result;
    }

    result.requiredBytes = layout.rowStride * height;
    if (result.requiredBytes > limits.maximumBytes) {
        result.error = ImageLayoutError::ByteLimitExceeded;
    }
    return result;
}

std::optional<RgbImage> RgbImage::create(ImageLayout layout,
                                         std::vector<std::uint8_t> bytes,
                                         const ImageSizeLimits& limits,
                                         ImageLayoutError* error)
{
    const ImageLayoutValidation validation = validateImageLayout(layout, limits);
    if (!validation) {
        setError(error, validation.error);
        return std::nullopt;
    }
    if (bytes.size() != validation.requiredBytes) {
        setError(error, ImageLayoutError::DataSizeMismatch);
        return std::nullopt;
    }

    setError(error, ImageLayoutError::None);
    return RgbImage(layout, std::move(bytes));
}

std::optional<RgbImage> RgbImage::createTightlyPacked(std::uint32_t width,
                                                      std::uint32_t height,
                                                      PixelFormat pixelFormat,
                                                      const ImageSizeLimits& limits,
                                                      ImageLayoutError* error)
{
    const std::size_t pixelSize = bytesPerPixel(pixelFormat);
    if (pixelSize == 0
        || static_cast<std::size_t>(width)
            > std::numeric_limits<std::size_t>::max() / pixelSize) {
        setError(error, pixelSize == 0 ? ImageLayoutError::UnsupportedPixelFormat
                                       : ImageLayoutError::ByteSizeOverflow);
        return std::nullopt;
    }

    const ImageLayout layout{
        .width = width,
        .height = height,
        .pixelFormat = pixelFormat,
        .rowStride = static_cast<std::size_t>(width) * pixelSize,
    };
    const ImageLayoutValidation validation = validateImageLayout(layout, limits);
    if (!validation) {
        setError(error, validation.error);
        return std::nullopt;
    }

    return create(layout, std::vector<std::uint8_t>(validation.requiredBytes), limits, error);
}

std::span<const std::uint8_t> RgbImage::row(std::uint32_t y) const
{
    if (y >= layout_.height) {
        throw std::out_of_range("RGB image row is outside the image");
    }
    return std::span<const std::uint8_t>(bytes_).subspan(
        static_cast<std::size_t>(y) * layout_.rowStride, layout_.rowStride);
}

std::span<std::uint8_t> RgbImage::row(std::uint32_t y)
{
    if (y >= layout_.height) {
        throw std::out_of_range("RGB image row is outside the image");
    }
    return std::span<std::uint8_t>(bytes_).subspan(
        static_cast<std::size_t>(y) * layout_.rowStride, layout_.rowStride);
}

RgbImage::RgbImage(ImageLayout layout, std::vector<std::uint8_t> bytes)
    : layout_(layout)
    , bytes_(std::move(bytes))
{
}

} // namespace newconvert9918::core
