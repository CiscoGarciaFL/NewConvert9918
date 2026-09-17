#include "newconvert9918/imageio/ImageLoader.hpp"

#include <QBuffer>
#include <QColorSpace>
#include <QFile>
#include <QFileInfo>
#include <QImageIOHandler>
#include <QImageReader>

#include <algorithm>
#include <array>
#include <limits>
#include <mutex>
#include <span>
#include <vector>

namespace newconvert9918::imageio {
namespace {

ImageLoadResult failure(QString message, ImageSourceFormat format = ImageSourceFormat::Unknown)
{
    ImageLoadMetadata metadata;
    metadata.format = format;
    return {.image = std::nullopt, .metadata = std::move(metadata), .error = std::move(message)};
}

ImageSourceFormat rasterFormat(const QByteArray& format)
{
    const QByteArray lower = format.toLower();
    if (lower == "png") return ImageSourceFormat::Png;
    if (lower == "jpg" || lower == "jpeg") return ImageSourceFormat::Jpeg;
    if (lower == "bmp") return ImageSourceFormat::Bmp;
    if (lower == "gif") return ImageSourceFormat::Gif;
    if (lower == "tif" || lower == "tiff") return ImageSourceFormat::Tiff;
    if (lower == "webp") return ImageSourceFormat::WebP;
    return ImageSourceFormat::Unknown;
}

QString displayName(ImageSourceFormat format)
{
    switch (format) {
    case ImageSourceFormat::Png: return QStringLiteral("PNG");
    case ImageSourceFormat::Jpeg: return QStringLiteral("JPEG");
    case ImageSourceFormat::Bmp: return QStringLiteral("BMP");
    case ImageSourceFormat::Gif: return QStringLiteral("GIF");
    case ImageSourceFormat::Tiff: return QStringLiteral("TIFF");
    case ImageSourceFormat::WebP: return QStringLiteral("WebP");
    case ImageSourceFormat::Pcx: return QStringLiteral("PCX");
    case ImageSourceFormat::TiArtist: return QStringLiteral("TI Artist");
    case ImageSourceFormat::MsxScreen2: return QStringLiteral("MSX Screen 2");
    case ImageSourceFormat::ColecoCvPaint: return QStringLiteral("Coleco CVPaint");
    case ImageSourceFormat::AdamPowerPaint: return QStringLiteral("Adam PowerPaint");
    case ImageSourceFormat::AdamHgr: return QStringLiteral("Adam HGR");
    case ImageSourceFormat::Clipboard: return QStringLiteral("Clipboard image");
    case ImageSourceFormat::Unknown: break;
    }
    return QStringLiteral("Unknown");
}

bool dimensionsAllowed(const QSize& size, const core::ImageSizeLimits& limits)
{
    if (!size.isValid() || size.width() <= 0 || size.height() <= 0
        || static_cast<std::uint64_t>(size.width()) > limits.maximumWidth
        || static_cast<std::uint64_t>(size.height()) > limits.maximumHeight) {
        return false;
    }
    const std::uint64_t pixels = static_cast<std::uint64_t>(size.width())
        * static_cast<std::uint64_t>(size.height());
    return pixels <= limits.maximumPixels && pixels <= limits.maximumBytes / 4U;
}

ImageLoadResult fromQImage(QImage source,
                           ImageLoadMetadata metadata,
                           const ImageLoadLimits& limits)
{
    if (source.isNull()) {
        return failure(QStringLiteral("The decoded image is empty."), metadata.format);
    }
    if (!dimensionsAllowed(source.size(), limits.decodedImage)) {
        return failure(QStringLiteral("The decoded image exceeds the configured limits."),
                       metadata.format);
    }
    metadata.sourceHadAlpha = source.hasAlphaChannel();
    if (source.colorSpace().isValid()) {
        metadata.colorSpace = source.colorSpace().description();
        if (source.colorSpace() != QColorSpace::SRgb) {
            source.convertToColorSpace(QColorSpace::SRgb);
        }
    } else {
        metadata.colorSpace = QStringLiteral("untagged (assumed sRGB)");
        source.setColorSpace(QColorSpace::SRgb);
    }
    source = source.convertToFormat(metadata.sourceHadAlpha
            ? QImage::Format_RGBA8888 : QImage::Format_RGB888);
    if (source.isNull()) {
        return failure(QStringLiteral("The image could not be converted to RGB pixels."),
                       metadata.format);
    }

    const core::PixelFormat pixelFormat = metadata.sourceHadAlpha
        ? core::PixelFormat::Rgba8888 : core::PixelFormat::Rgb888;
    const core::ImageLayout layout{
        static_cast<std::uint32_t>(source.width()),
        static_cast<std::uint32_t>(source.height()),
        pixelFormat,
        static_cast<std::size_t>(source.bytesPerLine()),
    };
    const qsizetype byteCount = source.sizeInBytes();
    if (byteCount < 0 || static_cast<std::uint64_t>(byteCount)
        > std::numeric_limits<std::size_t>::max()) {
        return failure(QStringLiteral("The decoded image byte size is invalid."), metadata.format);
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(byteCount));
    std::copy_n(source.constBits(), bytes.size(), bytes.begin());
    auto image = core::RgbImage::create(layout, std::move(bytes), limits.decodedImage);
    if (!image) {
        return failure(QStringLiteral("The decoded image exceeds the configured limits."),
                       metadata.format);
    }
    metadata.formatName = displayName(metadata.format);
    return {.image = std::move(image), .metadata = std::move(metadata), .error = {}};
}

ImageLoadResult fromCore(core::ImageDecodeResult decoded, ImageSourceFormat format)
{
    if (!decoded) {
        return failure(QString::fromStdString(decoded.message), format);
    }
    const bool hasAlpha = decoded.image->pixelFormat() == core::PixelFormat::Rgba8888;
    return {
        .image = std::move(decoded.image),
        .metadata = {
            .format = format,
            .formatName = displayName(format),
            .colorSpace = QStringLiteral("format-defined sRGB-compatible palette"),
            .sourceHadAlpha = hasAlpha,
        },
        .error = {},
    };
}

QByteArray readFileLimited(const QString& path,
                           std::size_t maximumBytes,
                           QString& error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("The file could not be opened: %1").arg(file.errorString());
        return {};
    }
    if (file.size() < 0 || static_cast<std::uint64_t>(file.size()) > maximumBytes) {
        error = QStringLiteral("The source file exceeds the configured input-size limit.");
        return {};
    }
    QByteArray bytes = file.readAll();
    if (bytes.size() != file.size()) {
        error = QStringLiteral("The source file could not be read completely.");
        return {};
    }
    return bytes;
}

bool isTiFilesHeader(std::span<const std::uint8_t> bytes)
{
    constexpr std::array<std::uint8_t, 8> signature{7, 'T', 'I', 'F', 'I', 'L', 'E', 'S'};
    return bytes.size() >= 128U && std::equal(signature.begin(), signature.end(), bytes.begin());
}

bool isV9t9Header(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() < 128U) return false;
    return std::all_of(bytes.begin(), bytes.begin() + 10, [](std::uint8_t value) {
        return value == ' ' || (value >= 0x21U && value <= 0x7eU);
    });
}

std::optional<QByteArray> legacyPayload(const QByteArray& file,
                                        std::span<const std::size_t> acceptedSizes)
{
    const auto bytes = std::span(reinterpret_cast<const std::uint8_t*>(file.constData()),
                                 static_cast<std::size_t>(file.size()));
    if (std::ranges::find(acceptedSizes, bytes.size()) != acceptedSizes.end()) {
        return file;
    }
    if (isTiFilesHeader(bytes) || isV9t9Header(bytes)) {
        const std::size_t payloadSize = bytes.size() - 128U;
        if (std::ranges::find(acceptedSizes, payloadSize) != acceptedSizes.end()) {
            return file.sliced(128, static_cast<qsizetype>(payloadSize));
        }
    }
    return std::nullopt;
}

std::optional<QByteArray> decodeTiRle(std::span<const std::uint8_t> encoded,
                                      std::size_t decodedSize)
{
    QByteArray decoded;
    decoded.reserve(static_cast<qsizetype>(decodedSize));
    std::size_t cursor = 0;
    while (static_cast<std::size_t>(decoded.size()) < decodedSize
           && cursor < encoded.size()) {
        const std::uint8_t command = encoded[cursor++];
        const std::size_t count = command & 0x7fU;
        if (count == 0 || count > decodedSize - static_cast<std::size_t>(decoded.size())) {
            return std::nullopt;
        }
        if ((command & 0x80U) != 0) {
            if (cursor >= encoded.size()) return std::nullopt;
            decoded.append(static_cast<qsizetype>(count), static_cast<char>(encoded[cursor++]));
        } else {
            if (count > encoded.size() - cursor) return std::nullopt;
            decoded.append(reinterpret_cast<const char*>(encoded.data() + cursor),
                           static_cast<qsizetype>(count));
            cursor += count;
        }
    }
    return static_cast<std::size_t>(decoded.size()) == decodedSize
        ? std::optional<QByteArray>(std::move(decoded)) : std::nullopt;
}

std::optional<QByteArray> legacyTablePayload(const QByteArray& file)
{
    constexpr std::array<std::size_t, 1> tableSize{6144U};
    if (auto uncompressed = legacyPayload(file, tableSize)) return uncompressed;

    const auto allBytes = std::span(reinterpret_cast<const std::uint8_t*>(file.constData()),
                                    static_cast<std::size_t>(file.size()));
    const std::size_t offset = isTiFilesHeader(allBytes) || isV9t9Header(allBytes)
        ? 128U : 0U;
    return decodeTiRle(allBytes.subspan(offset), tableSize.front());
}

QString tiCompanion(const QString& path, QChar role)
{
    QString companion = path;
    if (!companion.isEmpty()) {
        companion[companion.size() - 1] = companion.back().isLower()
            ? role.toLower() : role.toUpper();
    }
    return companion;
}

ImageLoadResult loadTiArtist(const QString& selectedPath, const ImageLoadLimits& limits)
{
    const QString patternPath = tiCompanion(selectedPath, QLatin1Char('P'));
    QString error;
    const QByteArray patternFile = readFileLimited(patternPath, limits.maximumInputBytes, error);
    if (!error.isEmpty()) return failure(error, ImageSourceFormat::TiArtist);
    auto pattern = legacyTablePayload(patternFile);
    if (!pattern) {
        return failure(QStringLiteral("TI Artist pattern data must contain 6144 bytes."),
                       ImageSourceFormat::TiArtist);
    }

    QByteArray colors(6144, static_cast<char>(0x1f));
    const QString colorPath = tiCompanion(selectedPath, QLatin1Char('C'));
    if (QFileInfo::exists(colorPath)) {
        error.clear();
        const QByteArray colorFile = readFileLimited(colorPath, limits.maximumInputBytes, error);
        if (!error.isEmpty()) return failure(error, ImageSourceFormat::TiArtist);
        auto payload = legacyTablePayload(colorFile);
        if (!payload) {
            return failure(QStringLiteral("TI Artist color data must contain 6144 bytes."),
                           ImageSourceFormat::TiArtist);
        }
        colors = std::move(*payload);
    }

    QByteArray palette;
    QByteArray multicolor;
    const QString auxiliaryPath = tiCompanion(selectedPath, QLatin1Char('M'));
    if (QFileInfo::exists(auxiliaryPath)) {
        error.clear();
        const QByteArray auxiliaryFile = readFileLimited(
            auxiliaryPath, limits.maximumInputBytes, error);
        if (!error.isEmpty()) return failure(error, ImageSourceFormat::TiArtist);
        constexpr std::array<std::size_t, 3> auxiliarySizes{32U, 2048U, 6144U};
        auto payload = legacyPayload(auxiliaryFile, auxiliarySizes);
        if (!payload) {
            return failure(QStringLiteral("TI Artist auxiliary data has an unsupported size."),
                           ImageSourceFormat::TiArtist);
        }
        if (payload->size() == 2048) multicolor = std::move(*payload);
        else palette = std::move(*payload);
    }
    const auto asBytes = [](const QByteArray& bytes) {
        return std::span(reinterpret_cast<const std::uint8_t*>(bytes.constData()),
                         static_cast<std::size_t>(bytes.size()));
    };
    return fromCore(core::decodeGraphics2(
        asBytes(*pattern), asBytes(colors), asBytes(palette), asBytes(multicolor),
        limits.decodedImage), ImageSourceFormat::TiArtist);
}

} // namespace

ImageLoadResult loadRasterData(const QByteArray& bytes,
                               const QByteArray& formatHint,
                               const ImageLoadLimits& limits)
{
    if (bytes.isEmpty()) return failure(QStringLiteral("The source data is empty."));
    if (static_cast<std::uint64_t>(bytes.size()) > limits.maximumInputBytes) {
        return failure(QStringLiteral("The source data exceeds the configured input-size limit."));
    }
    QBuffer buffer;
    buffer.setData(bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, formatHint);
    reader.setDecideFormatFromContent(true);
    reader.setAutoTransform(true);
    const QSize declaredSize = reader.size();
    if (declaredSize.isValid() && !dimensionsAllowed(declaredSize, limits.decodedImage)) {
        return failure(QStringLiteral("The declared image dimensions exceed the configured limits."));
    }
    const QByteArray detectedFormat = reader.format().toLower();
    const ImageSourceFormat format = rasterFormat(
        detectedFormat.isEmpty() ? formatHint : detectedFormat);
    if (format == ImageSourceFormat::Unknown) {
        return failure(QStringLiteral("The raster format is unsupported or unavailable in Qt."));
    }
    const int count = reader.imageCount();
    QImage image;
    {
        // QImageReader's decoder-side allocation guard is process-global. Keep
        // the tighter setting scoped and serialize our reads so concurrent
        // preview requests cannot restore each other's value out of order.
        static std::mutex allocationLimitMutex;
        const std::lock_guard lock(allocationLimitMutex);
        const int previousLimit = QImageReader::allocationLimit();
        constexpr std::size_t mebibyte = 1024U * 1024U;
        const std::size_t requested = std::max<std::size_t>(
            1U, limits.decodedImage.maximumBytes / mebibyte
                + (limits.decodedImage.maximumBytes % mebibyte != 0U));
        const int requestedLimit = static_cast<int>(std::min<std::size_t>(
            requested, static_cast<std::size_t>(std::numeric_limits<int>::max())));
        QImageReader::setAllocationLimit(
            previousLimit > 0 ? std::min(previousLimit, requestedLimit) : requestedLimit);
        image = reader.read();
        QImageReader::setAllocationLimit(previousLimit);
    }
    if (image.isNull()) {
        return failure(QStringLiteral("The image could not be decoded: %1").arg(reader.errorString()),
                       format);
    }
    ImageLoadMetadata metadata;
    metadata.format = format;
    metadata.formatName = displayName(format);
    metadata.orientationApplied = reader.transformation()
        != QImageIOHandler::TransformationNone;
    metadata.animated = reader.supportsAnimation();
    metadata.frameCount = count > 0 ? count : 1;
    return fromQImage(image, std::move(metadata), limits);
}

ImageLoadResult loadImageFile(const QString& path, const ImageLoadLimits& limits)
{
    const QFileInfo info(path);
    const QString suffix = info.suffix().toLower();
    if (!info.exists() || !info.isFile()) {
        return failure(QStringLiteral("The selected source file does not exist."));
    }
    if (suffix == QStringLiteral("tiap") || suffix == QStringLiteral("tiac")
        || suffix == QStringLiteral("tiam") || path.endsWith(QStringLiteral("_P"), Qt::CaseInsensitive)
        || path.endsWith(QStringLiteral("_C"), Qt::CaseInsensitive)
        || path.endsWith(QStringLiteral("_M"), Qt::CaseInsensitive)) {
        return loadTiArtist(path, limits);
    }

    QString error;
    const QByteArray bytes = readFileLimited(path, limits.maximumInputBytes, error);
    if (!error.isEmpty()) return failure(error);
    const auto raw = std::span(reinterpret_cast<const std::uint8_t*>(bytes.constData()),
                               static_cast<std::size_t>(bytes.size()));
    if (suffix == QStringLiteral("pcx")) {
        return fromCore(core::decodePcx(raw, limits.decodedImage), ImageSourceFormat::Pcx);
    }
    if (suffix == QStringLiteral("sc2")) {
        return fromCore(core::decodeMsxScreen2(raw, limits.decodedImage),
                        ImageSourceFormat::MsxScreen2);
    }
    if (suffix == QStringLiteral("pc")) {
        return fromCore(core::decodeColecoCvPaint(raw, limits.decodedImage),
                        ImageSourceFormat::ColecoCvPaint);
    }
    if (suffix == QStringLiteral("pp")) {
        return fromCore(core::decodeAdamPowerPaint(raw, limits.decodedImage),
                        ImageSourceFormat::AdamPowerPaint);
    }
    if (suffix == QStringLiteral("hgr") || suffix == QStringLiteral("hgrh")) {
        return fromCore(core::decodeAdamHgr(raw, limits.decodedImage),
                        ImageSourceFormat::AdamHgr);
    }
    return loadRasterData(bytes, suffix.toLatin1(), limits);
}

ImageLoadResult loadClipboardImage(const QImage& image, const ImageLoadLimits& limits)
{
    ImageLoadMetadata metadata;
    metadata.format = ImageSourceFormat::Clipboard;
    return fromQImage(image, std::move(metadata), limits);
}

QImage toQImage(const core::RgbImage& image)
{
    const QImage::Format format = image.pixelFormat() == core::PixelFormat::Rgba8888
        ? QImage::Format_RGBA8888 : QImage::Format_RGB888;
    QImage view(image.bytes().data(),
                static_cast<int>(image.width()),
                static_cast<int>(image.height()),
                static_cast<qsizetype>(image.rowStride()),
                format);
    return view.copy();
}

} // namespace newconvert9918::imageio
