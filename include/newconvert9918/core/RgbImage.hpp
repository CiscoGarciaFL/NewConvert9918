#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace newconvert9918::core {

enum class PixelFormat : std::uint8_t {
    Rgb888,
    Rgba8888,
};

[[nodiscard]] constexpr std::size_t bytesPerPixel(PixelFormat format)
{
    switch (format) {
    case PixelFormat::Rgb888:
        return 3;
    case PixelFormat::Rgba8888:
        return 4;
    }
    return 0;
}

struct ImageSizeLimits {
    std::uint32_t maximumWidth{16'384};
    std::uint32_t maximumHeight{16'384};
    std::size_t maximumPixels{64U * 1024U * 1024U};
    std::size_t maximumBytes{256U * 1024U * 1024U};
};

struct ImageLayout {
    std::uint32_t width{};
    std::uint32_t height{};
    PixelFormat pixelFormat{PixelFormat::Rgba8888};
    std::size_t rowStride{};
};

enum class ImageLayoutError : std::uint8_t {
    None,
    ZeroWidth,
    ZeroHeight,
    UnsupportedPixelFormat,
    DimensionLimitExceeded,
    PixelLimitExceeded,
    RowStrideTooSmall,
    ByteSizeOverflow,
    ByteLimitExceeded,
    DataSizeMismatch,
};

struct ImageLayoutValidation {
    ImageLayoutError error{ImageLayoutError::None};
    std::size_t minimumRowBytes{};
    std::size_t requiredBytes{};

    [[nodiscard]] explicit constexpr operator bool() const
    {
        return error == ImageLayoutError::None;
    }
};

[[nodiscard]] ImageLayoutValidation
validateImageLayout(const ImageLayout& layout, const ImageSizeLimits& limits = {});

class RgbImage final {
public:
    [[nodiscard]] static std::optional<RgbImage>
    create(ImageLayout layout,
           std::vector<std::uint8_t> bytes,
           const ImageSizeLimits& limits = {},
           ImageLayoutError* error = nullptr);

    [[nodiscard]] static std::optional<RgbImage>
    createTightlyPacked(std::uint32_t width,
                        std::uint32_t height,
                        PixelFormat pixelFormat,
                        const ImageSizeLimits& limits = {},
                        ImageLayoutError* error = nullptr);

    [[nodiscard]] std::uint32_t width() const { return layout_.width; }
    [[nodiscard]] std::uint32_t height() const { return layout_.height; }
    [[nodiscard]] PixelFormat pixelFormat() const { return layout_.pixelFormat; }
    [[nodiscard]] std::size_t rowStride() const { return layout_.rowStride; }
    [[nodiscard]] std::size_t minimumRowBytes() const
    {
        return static_cast<std::size_t>(layout_.width) * bytesPerPixel(layout_.pixelFormat);
    }
    [[nodiscard]] const ImageLayout& layout() const { return layout_; }
    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const { return bytes_; }
    [[nodiscard]] std::vector<std::uint8_t>& bytes() { return bytes_; }
    [[nodiscard]] std::span<const std::uint8_t> row(std::uint32_t y) const;
    [[nodiscard]] std::span<std::uint8_t> row(std::uint32_t y);

private:
    RgbImage(ImageLayout layout, std::vector<std::uint8_t> bytes);

    ImageLayout layout_;
    std::vector<std::uint8_t> bytes_;
};

} // namespace newconvert9918::core
