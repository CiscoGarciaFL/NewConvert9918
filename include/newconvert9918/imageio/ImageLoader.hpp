#pragma once

#include "newconvert9918/core/ImageDecoding.hpp"

#include <QByteArray>
#include <QImage>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <optional>

namespace newconvert9918::imageio {

enum class ImageSourceFormat : std::uint8_t {
    Unknown,
    Png,
    Jpeg,
    Bmp,
    Gif,
    Tiff,
    WebP,
    Pcx,
    TiArtist,
    MsxScreen2,
    ColecoCvPaint,
    AdamPowerPaint,
    AdamHgr,
    Clipboard,
};

struct ImageLoadLimits {
    std::size_t maximumInputBytes{64U * 1024U * 1024U};
    core::ImageSizeLimits decodedImage{};
};

struct ImageLoadMetadata {
    ImageSourceFormat format{ImageSourceFormat::Unknown};
    QString formatName;
    QString colorSpace;
    bool sourceHadAlpha{};
    bool orientationApplied{};
    bool animated{};
    int frameCount{1};
};

struct ImageLoadResult {
    std::optional<core::RgbImage> image;
    ImageLoadMetadata metadata;
    QString error;

    [[nodiscard]] explicit operator bool() const { return image.has_value(); }
};

// Common raster policy: apply embedded orientation, convert tagged pixels to
// sRGB, preserve alpha, and decode only the first animation frame.
[[nodiscard]] ImageLoadResult loadImageFile(
    const QString& path,
    const ImageLoadLimits& limits = {});
[[nodiscard]] ImageLoadResult loadRasterData(
    const QByteArray& bytes,
    const QByteArray& formatHint,
    const ImageLoadLimits& limits = {});
[[nodiscard]] ImageLoadResult loadClipboardImage(
    const QImage& image,
    const ImageLoadLimits& limits = {});

[[nodiscard]] QImage toQImage(const core::RgbImage& image);

} // namespace newconvert9918::imageio
