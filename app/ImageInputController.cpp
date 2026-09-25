#include "ImageInputController.hpp"

#include "ConversionPipeline.hpp"

#include "newconvert9918/core/Dithering.hpp"
#include "newconvert9918/core/ImageTransform.hpp"
#include "newconvert9918/imageio/ExportWriter.hpp"

#include <QBuffer>
#include <QClipboard>
#include <QColor>
#include <QCoreApplication>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QMetaObject>
#include <QMimeData>
#include <QPointer>
#include <QRegularExpression>
#include <QSettings>
#include <QThreadPool>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <span>
#include <unordered_set>
#include <utility>

namespace {

using namespace newconvert9918;

constexpr std::size_t maximumDrawingHistoryEntries = 64;
constexpr std::size_t maximumDrawingHistoryBytes = 128U * 1024U * 1024U;

void trimDrawingHistory(std::vector<std::shared_ptr<core::RgbImage>>& history)
{
    std::size_t totalBytes = 0;
    for (const auto& image : history) {
        if (image) totalBytes += image->bytes().size();
    }
    while (history.size() > 1U
           && (history.size() > maximumDrawingHistoryEntries
               || totalBytes > maximumDrawingHistoryBytes)) {
        if (history.front()) totalBytes -= history.front()->bytes().size();
        history.erase(history.begin());
    }
}

QString dataUrl(const QImage& image)
{
    QByteArray encoded;
    QBuffer buffer(&encoded);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) return {};
    return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(encoded.toBase64());
}

QString dataUrl(const core::RgbImage& image)
{
    return dataUrl(imageio::toQImage(image));
}

QString modeName(core::ConversionMode mode)
{
    switch (mode) {
    case core::ConversionMode::Bitmap9918: return QStringLiteral("Bitmap 9918A");
    case core::ConversionMode::GreyscaleBitmap9918:
        return QStringLiteral("Greyscale Bitmap 9918A");
    case core::ConversionMode::BlackAndWhiteBitmap9918:
        return QStringLiteral("Black-and-White Bitmap 9918A");
    case core::ConversionMode::Multicolor9918: return QStringLiteral("Multicolor 9918");
    case core::ConversionMode::DualMulticolor9918:
        return QStringLiteral("Dual Multicolor 9918");
    case core::ConversionMode::HalfMulticolor9918:
        return QStringLiteral("Half Multicolor 9918A");
    case core::ConversionMode::BitmapColorOnly9918:
        return QStringLiteral("Bitmap Color Only 9918A");
    case core::ConversionMode::PalettedBitmapF18A:
        return QStringLiteral("Paletted Bitmap F18A");
    case core::ConversionMode::ScanlinePaletteBitmapF18A:
        return QStringLiteral("Scanline Palette Bitmap F18A");
    }
    return QStringLiteral("Unknown mode");
}

formats::ExportFormat exportFormatForIndex(int index)
{
    constexpr std::array availableFormats{
        formats::ExportFormat::TiFiles,
        formats::ExportFormat::V9t9,
        formats::ExportFormat::Raw,
        formats::ExportFormat::Rle,
        formats::ExportFormat::MsxScreen2,
        formats::ExportFormat::ColecoCvPaint,
        formats::ExportFormat::AdamPowerPaint,
        formats::ExportFormat::AdamHgr,
        formats::ExportFormat::Png,
    };
    return availableFormats[static_cast<std::size_t>(
        std::clamp(index, 0, static_cast<int>(availableFormats.size()) - 1))];
}

core::ConversionResult runConversion(const core::ConversionRequest& request,
                                     core::ScalingFilter scalingFilter,
                                     core::ImageFillMode fillMode,
                                     int horizontalOffset,
                                     int verticalOffset,
                                     core::RgbColor backgroundColor,
                                     std::vector<core::RgbColor> workingColors,
                                     bool powerPaintFraming,
                                     bool sourceAlreadyFramed,
                                     core::ConversionProgressCallback progress)
{
    return appsupport::runConversion(
        request,
        {
            .scalingFilter = scalingFilter,
            .fillMode = fillMode,
            .horizontalOffset = horizontalOffset,
            .verticalOffset = verticalOffset,
            .backgroundColor = backgroundColor,
            .workingPalette = std::move(workingColors),
            .powerPaintFraming = powerPaintFraming,
            .sourceAlreadyFramed = sourceAlreadyFramed,
            .progress = std::move(progress),
        });
}

std::vector<core::RgbColor> decodeF18Palette(std::span<const std::uint8_t> bytes)
{
    std::vector<core::RgbColor> colors;
    for (std::size_t offset = 0; offset + 1U < bytes.size(); offset += 2U) {
        colors.push_back({
            static_cast<std::uint8_t>((bytes[offset] & 0x0fU) * 17U),
            static_cast<std::uint8_t>((bytes[offset + 1U] >> 4U) * 17U),
            static_cast<std::uint8_t>((bytes[offset + 1U] & 0x0fU) * 17U),
        });
    }
    return colors;
}

QString colorName(const core::RgbColor& color)
{
    return QStringLiteral("#%1%2%3")
        .arg(color.red, 2, 16, QLatin1Char('0'))
        .arg(color.green, 2, 16, QLatin1Char('0'))
        .arg(color.blue, 2, 16, QLatin1Char('0'))
        .toUpper();
}

std::vector<core::RgbColor> defaultWorkingColors()
{
    return appsupport::defaultWorkingPalette();
}

core::RgbColor nearestBackgroundColor(
    const QColor& requested,
    std::span<const core::RgbColor> colors)
{
    core::RgbColor nearest = colors.front();
    int nearestDistance = std::numeric_limits<int>::max();
    for (const auto& candidate : colors) {
        const int red = requested.red() - static_cast<int>(candidate.red);
        const int green = requested.green() - static_cast<int>(candidate.green);
        const int blue = requested.blue() - static_cast<int>(candidate.blue);
        const int distance = red * red + green * green + blue * blue;
        if (distance < nearestDistance) {
            nearest = candidate;
            nearestDistance = distance;
        }
    }
    return nearest;
}

std::vector<core::RgbColor> standardSourceSwatch()
{
    return {
        {0, 0, 0}, {64, 64, 64}, {128, 128, 128}, {192, 192, 192}, {255, 255, 255},
        {128, 0, 0}, {255, 0, 0}, {255, 128, 128}, {128, 64, 0}, {255, 128, 0},
        {255, 192, 128}, {128, 128, 0}, {255, 255, 0}, {255, 255, 128}, {0, 128, 0},
        {0, 255, 0}, {128, 255, 128}, {0, 128, 128}, {0, 255, 255}, {128, 255, 255},
        {0, 0, 128}, {0, 0, 255}, {128, 128, 255}, {128, 0, 128}, {255, 0, 255},
        {255, 128, 255}, {64, 32, 0}, {192, 96, 0}, {255, 192, 0}, {96, 64, 32},
        {160, 128, 64}, {224, 192, 128}, {32, 64, 96}, {64, 128, 192}, {128, 192, 224},
    };
}

std::vector<core::RgbColor> standardPcPalette()
{
    // The first two entries remain black and white so the picker keeps its
    // stable foreground/background convention.
    return {
        {0, 0, 0},       {255, 255, 255}, {128, 0, 0},   {0, 128, 0},
        {128, 128, 0},   {0, 0, 128},     {128, 0, 128}, {0, 128, 128},
        {192, 192, 192}, {128, 128, 128}, {255, 0, 0},   {0, 255, 0},
        {255, 255, 0},   {0, 0, 255},     {255, 0, 255}, {0, 255, 255},
    };
}

core::RgbColor darkerTone(core::RgbColor color)
{
    return {static_cast<std::uint8_t>(color.red * 0.72),
            static_cast<std::uint8_t>(color.green * 0.72),
            static_cast<std::uint8_t>(color.blue * 0.72)};
}

core::RgbColor lighterTone(core::RgbColor color)
{
    return {static_cast<std::uint8_t>(color.red + (255 - color.red) * 0.28),
            static_cast<std::uint8_t>(color.green + (255 - color.green) * 0.28),
            static_cast<std::uint8_t>(color.blue + (255 - color.blue) * 0.28)};
}

core::RgbColor middleTone(core::RgbColor color)
{
    return {static_cast<std::uint8_t>((color.red + 128) / 2),
            static_cast<std::uint8_t>((color.green + 128) / 2),
            static_cast<std::uint8_t>((color.blue + 128) / 2)};
}

std::vector<core::RgbColor> pcSpectrum(std::size_t count)
{
    const auto base = standardPcPalette();
    std::vector<core::RgbColor> colors = base;
    if (count >= 32U) {
        for (const auto color : base) colors.push_back(lighterTone(color));
    }
    if (count >= 64U) {
        for (const auto color : base) colors.push_back(darkerTone(color));
        for (const auto color : base) colors.push_back(middleTone(color));
    }
    return colors;
}

QVariantList colorList(const std::vector<core::RgbColor>& colors)
{
    QVariantList result;
    result.reserve(static_cast<qsizetype>(colors.size()));
    for (const auto& color : colors) {
        result.push_back(QColor(color.red, color.green, color.blue));
    }
    return result;
}

struct SourceColorChoice {
    std::uint32_t rgb{};
    int hue{};
    int saturation{};
    int value{};
    bool neutral{};
};

SourceColorChoice sourceColorChoice(std::uint32_t rgb)
{
    const QColor color(static_cast<int>((rgb >> 16U) & 0xffU),
                       static_cast<int>((rgb >> 8U) & 0xffU),
                       static_cast<int>(rgb & 0xffU));
    return {.rgb = rgb,
            .hue = color.hsvHue(),
            .saturation = color.hsvSaturation(),
            .value = color.value(),
            .neutral = color.hsvSaturation() <= 8};
}

bool sourceColorLess(const SourceColorChoice& left, const SourceColorChoice& right)
{
    if (left.neutral != right.neutral) return left.neutral;
    if (left.neutral) {
        if (left.value != right.value) return left.value < right.value;
        return left.rgb < right.rgb;
    }
    if (left.hue != right.hue) return left.hue < right.hue;
    if (left.value != right.value) return left.value < right.value;
    if (left.saturation != right.saturation) return left.saturation > right.saturation;
    return left.rgb < right.rgb;
}

} // namespace

ImageInputController::ImageInputController(QObject* parent)
    : QObject(parent), workingPalette_(defaultWorkingColors())
{
    debounceTimer_.setSingleShot(true);
    debounceTimer_.setInterval(160);
    connect(&debounceTimer_, &QTimer::timeout, this, &ImageInputController::startConversion);
    undoCoalesceTimer_.setSingleShot(true);
    undoCoalesceTimer_.setInterval(500);
    loadSettings();
}

ImageInputController::~ImageInputController()
{
    jobs_.cancelCurrent();
}

bool ImageInputController::hasConversion() const
{
    return result_.has_value() && result_->succeeded() && result_->preview.has_value()
        && result_->target.has_value();
}

int ImageInputController::conversionMode() const
{
    return static_cast<int>(settings_.mode);
}

int ImageInputController::ditherMode() const
{
    return static_cast<int>(settings_.dither);
}

QColor ImageInputController::backgroundColor() const
{
    return QColor(backgroundColor_.red, backgroundColor_.green, backgroundColor_.blue);
}

QColor ImageInputController::foregroundColor() const
{
    return QColor(foregroundColor_.red, foregroundColor_.green, foregroundColor_.blue);
}

QVariantList ImageInputController::backgroundPaletteColors() const
{
    QVariantList colors;
    for (const auto& color : workingPalette_) {
        colors.push_back(colorName(color));
    }
    return colors;
}

QVariantList ImageInputController::workingPaletteColors() const
{
    return backgroundPaletteColors();
}

void ImageInputController::refreshSourceColorChoices()
{
    sourceSpectrum16Colors_.clear();
    sourceSpectrum32Colors_.clear();
    sourceSpectrum64Colors_.clear();
    sourceSwatchColors_.clear();
    sourceUsedColors_.clear();
    const core::RgbImage* colorSource = image_.get();
    if (colorSource == nullptr) {
        emit sourceColorsChanged();
        return;
    }

    sourceSpectrum16Colors_ = colorList(pcSpectrum(16));
    sourceSpectrum32Colors_ = colorList(pcSpectrum(32));
    sourceSpectrum64Colors_ = colorList(pcSpectrum(64));
    sourceSwatchColors_ = colorList(standardSourceSwatch());

    std::unordered_set<std::uint32_t> seenColors;
    std::vector<SourceColorChoice> uniqueColors;
    constexpr std::size_t maximumTrackedColors = 1'000'000;
    seenColors.reserve(std::min(maximumTrackedColors,
                                static_cast<std::size_t>(colorSource->width())
                                    * colorSource->height()));
    const auto collectColors = [&](const core::RgbImage& source) {
        const std::size_t channels = core::bytesPerPixel(source.pixelFormat());
        for (std::uint32_t y = 0; y < source.height(); ++y) {
            const auto row = source.row(y);
            for (std::uint32_t x = 0; x < source.width(); ++x) {
                const auto* pixel = row.data() + static_cast<std::size_t>(x) * channels;
                if (channels == 4 && pixel[3] == 0) continue;
                const std::uint32_t key = (static_cast<std::uint32_t>(pixel[0]) << 16U)
                    | (static_cast<std::uint32_t>(pixel[1]) << 8U) | pixel[2];
                if (seenColors.size() >= maximumTrackedColors) continue;
                if (seenColors.insert(key).second) {
                    uniqueColors.push_back(sourceColorChoice(key));
                }
            }
        }
    };
    collectColors(*colorSource);
    if (drawingLayer_) collectColors(*drawingLayer_);
    std::sort(uniqueColors.begin(), uniqueColors.end(), sourceColorLess);
    constexpr std::size_t maximumDisplayedColors = 4096;
    const std::size_t displayed = std::min(maximumDisplayedColors, uniqueColors.size());
    sourceUsedColors_.reserve(static_cast<qsizetype>(displayed));
    for (std::size_t index = 0; index < displayed; ++index) {
        const std::size_t sourceIndex = uniqueColors.size() <= maximumDisplayedColors
            ? index
            : index * (uniqueColors.size() - 1) / (displayed - 1);
        const auto key = uniqueColors[sourceIndex].rgb;
        sourceUsedColors_.push_back(QColor(
            static_cast<int>((key >> 16U) & 0xffU),
            static_cast<int>((key >> 8U) & 0xffU),
            static_cast<int>(key & 0xffU)));
    }
    emit sourceColorsChanged();
}

int ImageInputController::perceptualRedWeight() const
{
    return qRound(settings_.perceptualRedWeight * 100.0);
}

int ImageInputController::perceptualGreenWeight() const
{
    return qRound(settings_.perceptualGreenWeight * 100.0);
}

int ImageInputController::perceptualBlueWeight() const
{
    return qRound(settings_.perceptualBlueWeight * 100.0);
}

void ImageInputController::accept(imageio::ImageLoadResult result, QString sourceName)
{
    if (!result) {
        errorMessage_ = result.error;
        statusMessage_.clear();
        emit statusChanged();
        return;
    }

    image_ = std::make_shared<core::RgbImage>(std::move(*result.image));
    sourceStrokeActive_ = false;
    drawingLayer_.reset();
    clearDrawingHistory();
    horizontalOffset_ = 0;
    verticalOffset_ = 0;
    sourceName_ = std::move(sourceName);
    sourceDetails_ = tr("%1 — %2×%3 — %4%5")
        .arg(result.metadata.formatName)
        .arg(image_->width())
        .arg(image_->height())
        .arg(result.metadata.colorSpace)
        .arg(result.metadata.animated
                 ? tr(" — first of %1 frames").arg(result.metadata.frameCount)
                 : QString{});
    refreshSourcePreview();
    refreshSourceColorChoices();
    if (sourcePreview_.isEmpty()) {
        errorMessage_ = tr("The decoded image could not be prepared for display.");
        emit statusChanged();
        return;
    }
    errorMessage_.clear();
    statusMessage_ = tr("Image loaded. Preparing preview…");
    convertedPreview_.clear();
    conversionDetails_.clear();
    result_.reset();
    emit statusChanged();
    emit conversionChanged();
    scheduleConversion();
}

void ImageInputController::openUrl(const QUrl& url)
{
    if (!url.isLocalFile()) {
        errorMessage_ = tr("Only local image files can be opened.");
        emit statusChanged();
        return;
    }
    const QString path = url.toLocalFile();
    auto result = imageio::loadImageFile(path);
    if (result) sourcePath_ = QFileInfo(path).absoluteFilePath();
    accept(std::move(result), QFileInfo(path).fileName());
}

void ImageInputController::reloadSource()
{
    if (sourcePath_.isEmpty()) return;
    openUrl(QUrl::fromLocalFile(sourcePath_));
}

void ImageInputController::pasteClipboard()
{
    const QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard == nullptr || clipboard->mimeData() == nullptr
        || !clipboard->mimeData()->hasImage()) {
        errorMessage_ = tr("The clipboard does not contain an image.");
        emit statusChanged();
        return;
    }
    auto result = imageio::loadClipboardImage(clipboard->image());
    if (result) sourcePath_.clear();
    accept(std::move(result), tr("Clipboard image"));
}

void ImageInputController::ensureDrawingLayer()
{
    if (drawingLayer_) return;
    auto layer = core::RgbImage::createTightlyPacked(
        256, 192, core::PixelFormat::Rgba8888);
    if (!layer) return;
    std::fill(layer->bytes().begin(), layer->bytes().end(), 0);
    drawingLayer_ = std::make_shared<core::RgbImage>(std::move(*layer));
}

void ImageInputController::compositeDrawingLayer(core::RgbImage& canvas) const
{
    if (!drawingLayer_ || drawingLayer_->width() != canvas.width()
        || drawingLayer_->height() != canvas.height()) {
        return;
    }
    const std::size_t canvasChannels = core::bytesPerPixel(canvas.pixelFormat());
    for (std::uint32_t y = 0; y < canvas.height(); ++y) {
        auto canvasRow = canvas.row(y);
        const auto layerRow = drawingLayer_->row(y);
        for (std::uint32_t x = 0; x < canvas.width(); ++x) {
            const std::size_t canvasOffset = static_cast<std::size_t>(x) * canvasChannels;
            const std::size_t layerOffset = static_cast<std::size_t>(x) * 4U;
            const double layerAlpha = layerRow[layerOffset + 3U] / 255.0;
            if (layerAlpha <= 0.0) continue;
            if (layerAlpha >= 1.0) {
                std::copy_n(layerRow.begin() + static_cast<std::ptrdiff_t>(layerOffset),
                            3, canvasRow.begin()
                                + static_cast<std::ptrdiff_t>(canvasOffset));
                if (canvasChannels == 4U) canvasRow[canvasOffset + 3U] = 255;
                continue;
            }

            const double canvasAlpha = canvasChannels == 4U
                ? canvasRow[canvasOffset + 3U] / 255.0 : 1.0;
            const double outputAlpha = layerAlpha
                + canvasAlpha * (1.0 - layerAlpha);
            for (std::size_t channel = 0; channel < 3U; ++channel) {
                const double premultiplied = layerRow[layerOffset + channel] * layerAlpha
                    + canvasRow[canvasOffset + channel] * canvasAlpha
                        * (1.0 - layerAlpha);
                canvasRow[canvasOffset + channel] = static_cast<std::uint8_t>(
                    std::clamp(static_cast<int>(std::lround(
                                   outputAlpha > 0.0
                                       ? premultiplied / outputAlpha : 0.0)),
                               0, 255));
            }
            if (canvasChannels == 4U) {
                canvasRow[canvasOffset + 3U] = static_cast<std::uint8_t>(
                    std::clamp(static_cast<int>(std::lround(outputAlpha * 255.0)),
                               0, 255));
            }
        }
    }
}

void ImageInputController::refreshSourcePreview()
{
    framedSource_.reset();
    sourcePreview_.clear();
    if (image_) {
        const core::ImageTransformOptions options{
            .targetWidth = powerPaintFraming_ ? 240U : 256U,
            .targetHeight = powerPaintFraming_ ? 160U : 192U,
            .filter = scalingFilter_,
            .fillMode = fillMode_,
            .horizontalOffset = horizontalOffset_,
            .verticalOffset = verticalOffset_,
            .backgroundRed = backgroundColor_.red,
            .backgroundGreen = backgroundColor_.green,
            .backgroundBlue = backgroundColor_.blue,
        };
        auto transformed = core::transformImage(*image_, options);
        if (transformed) {
            if (powerPaintFraming_) {
                auto padded = core::RgbImage::createTightlyPacked(
                    256, 192, transformed.image->pixelFormat());
                if (padded) {
                    std::fill(padded->bytes().begin(), padded->bytes().end(), 0);
                    if (padded->pixelFormat() == core::PixelFormat::Rgba8888) {
                        for (std::size_t offset = 3; offset < padded->bytes().size(); offset += 4) {
                            padded->bytes()[offset] = 255;
                        }
                    }
                    for (std::uint32_t y = 0; y < 160; ++y) {
                        const auto sourceRow = transformed.image->row(y);
                        auto destinationRow = padded->row(y);
                        std::copy_n(sourceRow.begin(), transformed.image->minimumRowBytes(),
                                    destinationRow.begin());
                    }
                    transformed.image = std::move(padded);
                }
            }
            compositeDrawingLayer(*transformed.image);
            framedSource_ = std::move(*transformed.image);
            sourcePreview_ = dataUrl(*framedSource_);
        }
    }
    emit sourceChanged();
}

void ImageInputController::scheduleConversion()
{
    jobs_.cancelCurrent();
    debounceTimer_.stop();
    if (!image_) return;
    if (!autoUpdate_) {
        busy_ = false;
        conversionPending_ = true;
        statusMessage_ = tr("Changes are ready. Select Update to convert.");
        emit conversionChanged();
        emit statusChanged();
        return;
    }
    conversionPending_ = false;
    busy_ = true;
    statusMessage_ = tr("Updating preview…");
    emit conversionChanged();
    emit statusChanged();
    debounceTimer_.start();
}

void ImageInputController::startConversion()
{
    if (!image_) return;
    if (!framedSource_) refreshSourcePreview();
    if (!framedSource_) return;
    auto preparedSource = std::make_shared<core::RgbImage>(*framedSource_);
    auto request = jobs_.begin(std::move(preparedSource), settings_);
    const auto scalingFilter = scalingFilter_;
    const auto fillMode = fillMode_;
    const int horizontalOffset = horizontalOffset_;
    const int verticalOffset = verticalOffset_;
    const auto backgroundColor = backgroundColor_;
    const auto workingPalette = workingPalette_;
    const bool powerPaintFraming = powerPaintFraming_;
    QPointer<ImageInputController> guarded(this);
    core::ConversionProgressCallback progress;
    if (livePreview_) {
        const std::uint64_t generation = request.generation;
        progress = [guarded, generation](const core::RgbImage& preview,
                                         std::uint32_t completedRows,
                                         std::uint32_t totalRows) {
            const QString previewUrl = dataUrl(preview);
            const int percent = totalRows == 0
                ? 0
                : static_cast<int>(completedRows * 100U / totalRows);
            QMetaObject::invokeMethod(
                QCoreApplication::instance(),
                [guarded, generation, previewUrl, percent] {
                    if (!guarded || !guarded->livePreview_ || !guarded->busy_
                        || !guarded->jobs_.isCurrent(generation)) {
                        return;
                    }
                    guarded->convertedPreview_ = previewUrl;
                    guarded->statusMessage_ = tr("Converting… %1%").arg(percent);
                    emit guarded->conversionChanged();
                    emit guarded->statusChanged();
                },
                Qt::QueuedConnection);
        };
    }
    QThreadPool::globalInstance()->start(
        [guarded, request = std::move(request), scalingFilter, fillMode, horizontalOffset,
         verticalOffset, backgroundColor, workingPalette, powerPaintFraming,
         progress = std::move(progress)]() mutable {
            auto result = runConversion(request,
                                        scalingFilter,
                                        fillMode,
                                        horizontalOffset,
                                        verticalOffset,
                                        backgroundColor,
                                        std::move(workingPalette),
                                        powerPaintFraming,
                                        true,
                                        std::move(progress));
            QMetaObject::invokeMethod(
                QCoreApplication::instance(),
                [guarded, request = std::move(request), result = std::move(result)]() mutable {
                    if (guarded) guarded->publishConversion(request, std::move(result));
                },
                Qt::QueuedConnection);
        });
}

void ImageInputController::publishConversion(const core::ConversionRequest& request,
                                             core::ConversionResult result)
{
    result = jobs_.finalize(request, std::move(result));
    if (!jobs_.accepts(result)) return;

    busy_ = false;
    if (!result.succeeded() || !result.preview || !result.target) {
        errorMessage_ = tr("Conversion failed.");
        for (const auto& diagnostic : result.diagnostics) {
            if (diagnostic.severity == core::DiagnosticSeverity::Error) {
                errorMessage_ = QString::fromStdString(diagnostic.message);
                break;
            }
        }
        statusMessage_.clear();
        result_.reset();
        convertedPreview_.clear();
        conversionDetails_.clear();
    } else {
        convertedPreview_ = dataUrl(*result.preview);
        const std::size_t byteCount = std::accumulate(
            result.target->tables.begin(), result.target->tables.end(), std::size_t{},
            [](std::size_t total, const core::TargetMemoryTable& table) {
                return total + table.bytes.size();
            });
        conversionDetails_ = tr("%1 — 256×192 — %2 target bytes")
                                 .arg(modeName(result.target->mode))
                                 .arg(byteCount);
        errorMessage_.clear();
        statusMessage_ = tr("Preview is current.");
        result_ = std::move(result);
        updatePaletteInspection();
        updateExportSummary();
    }
    emit conversionChanged();
    emit exportChanged();
    emit statusChanged();
}

void ImageInputController::updatePaletteInspection()
{
    paletteColors_.clear();
    palettePreview_.clear();
    if (!hasConversion()) return;

    std::vector<core::RgbColor> colors;
    const auto& target = *result_->target;
    const auto scanline = std::ranges::find(
        target.tables, core::TargetTableRole::ScanlinePalettes,
        &core::TargetMemoryTable::role);
    if (scanline != target.tables.end() && scanline->bytes.size() == 6144U) {
        QImage visualization(16, 192, QImage::Format_RGB888);
        for (int row = 0; row < 192; ++row) {
            const auto rowColors = decodeF18Palette(
                std::span(scanline->bytes).subspan(static_cast<std::size_t>(row) * 32U, 32U));
            if (row == 0) colors = rowColors;
            for (int column = 0; column < 16; ++column) {
                const auto& color = rowColors[static_cast<std::size_t>(column)];
                visualization.setPixelColor(column, row,
                                            QColor(color.red, color.green, color.blue));
            }
        }
        palettePreview_ = dataUrl(visualization);
    } else {
        const auto fixed = std::ranges::find(
            target.tables, core::TargetTableRole::Palette, &core::TargetMemoryTable::role);
        if (fixed != target.tables.end()) {
            colors = decodeF18Palette(fixed->bytes);
        } else if (target.palette) {
            colors.assign(target.palette->colors().begin(), target.palette->colors().end());
        }
    }
    for (const auto& color : colors) paletteColors_.push_back(colorName(color));
}

QString ImageInputController::exportBaseName() const
{
    QString base = QFileInfo(sourceName_).completeBaseName();
    if (base.isEmpty()) base = QStringLiteral("image");
    base.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")), QStringLiteral("_"));
    return base;
}

formats::GeneratedFileManifest ImageInputController::exportManifest() const
{
    if (!hasConversion()) {
        formats::GeneratedFileManifest manifest;
        manifest.format = exportFormatForIndex(exportFormat_);
        manifest.error = formats::ExportError::MissingTarget;
        manifest.message = "Complete a conversion before exporting.";
        return manifest;
    }
    const std::string baseName = exportBaseName().toStdString();
    const auto format = exportFormatForIndex(exportFormat_);
    const formats::ExportRequest request{
        .format = format,
        .baseName = baseName,
        .target = &*result_->target,
        .preview = &*result_->preview,
    };
    return format == formats::ExportFormat::Png ? imageio::generatePngExport(request)
                                                 : formats::generateExport(request);
}

void ImageInputController::updateExportSummary()
{
    if (!hasConversion()) {
        outputSummary_ = tr("Load an image to see the generated files.");
        emit exportChanged();
        return;
    }
    const auto manifest = exportManifest();
    if (!manifest) {
        outputSummary_ = tr("Unavailable: %1").arg(QString::fromStdString(manifest.message));
        emit exportChanged();
        return;
    }
    QStringList lines;
    std::size_t total = 0;
    for (const auto& file : manifest.files) {
        total += file.bytes.size();
        lines.push_back(tr("%1 — %2 bytes")
                            .arg(QString::fromStdString(file.fileName))
                            .arg(file.bytes.size()));
    }
    lines.push_back(tr("Total: %1 file(s), %2 bytes").arg(manifest.files.size()).arg(total));
    outputSummary_ = lines.join(QLatin1Char('\n'));
    emit exportChanged();
}

void ImageInputController::exportToDirectory(const QUrl& directory)
{
    if (!directory.isLocalFile()) {
        errorMessage_ = tr("Choose a local export folder.");
        emit statusChanged();
        return;
    }
    auto manifest = exportManifest();
    if (!manifest) {
        errorMessage_ = QString::fromStdString(manifest.message);
        emit statusChanged();
        return;
    }
    const QString path = directory.toLocalFile();
    const auto written = imageio::writeExportManifest(path, manifest);
    if (written.status == imageio::ExportWriteStatus::WouldOverwrite) {
        pendingExportDirectory_ = path;
        pendingManifest_ = std::move(manifest);
        overwriteMessage_ = tr("The following files already exist:\n%1")
                                .arg(written.conflicts.join(QLatin1Char('\n')));
        emit exportChanged();
        return;
    }
    if (!written) {
        errorMessage_ = written.error;
    } else {
        errorMessage_.clear();
        statusMessage_ = tr("Exported %1 file(s).").arg(written.paths.size());
    }
    emit statusChanged();
}

void ImageInputController::confirmOverwrite()
{
    if (!pendingManifest_) return;
    const auto written = imageio::writeExportManifest(
        pendingExportDirectory_, *pendingManifest_, true);
    overwriteMessage_.clear();
    pendingManifest_.reset();
    pendingExportDirectory_.clear();
    if (!written) {
        errorMessage_ = written.error;
    } else {
        errorMessage_.clear();
        statusMessage_ = tr("Replaced %1 file(s).").arg(written.paths.size());
    }
    emit exportChanged();
    emit statusChanged();
}

void ImageInputController::cancelOverwrite()
{
    overwriteMessage_.clear();
    pendingManifest_.reset();
    pendingExportDirectory_.clear();
    statusMessage_ = tr("Export cancelled; no files were changed.");
    emit exportChanged();
    emit statusChanged();
}

void ImageInputController::updateConversion()
{
    if (!image_) return;
    jobs_.cancelCurrent();
    debounceTimer_.stop();
    conversionPending_ = false;
    busy_ = true;
    statusMessage_ = tr("Updating preview…");
    emit conversionChanged();
    emit statusChanged();
    startConversion();
}

void ImageInputController::nudgeSource(int horizontal, int vertical)
{
    const int newHorizontal = std::clamp(horizontalOffset_ + horizontal, -256, 256);
    const int newVertical = std::clamp(verticalOffset_ + vertical, -192, 192);
    if (newHorizontal == horizontalOffset_ && newVertical == verticalOffset_) return;
    recordUndo();
    horizontalOffset_ = newHorizontal;
    verticalOffset_ = newVertical;
    settingsWereChanged(true);
}

void ImageInputController::centerSource()
{
    if (horizontalOffset_ == 0 && verticalOffset_ == 0) return;
    recordUndo();
    horizontalOffset_ = 0;
    verticalOffset_ = 0;
    settingsWereChanged(true);
}

void ImageInputController::pickBackgroundColor(double normalizedX, double normalizedY)
{
    if (!framedSource_) return;
    normalizedX = std::clamp(normalizedX, 0.0, 1.0);
    normalizedY = std::clamp(normalizedY, 0.0, 1.0);
    const auto x = std::min<std::uint32_t>(
        framedSource_->width() - 1,
        static_cast<std::uint32_t>(normalizedX * framedSource_->width()));
    const auto y = std::min<std::uint32_t>(
        framedSource_->height() - 1,
        static_cast<std::uint32_t>(normalizedY * framedSource_->height()));
    const auto row = framedSource_->row(y);
    const std::size_t offset = static_cast<std::size_t>(x)
        * core::bytesPerPixel(framedSource_->pixelFormat());
    const QColor picked(row[offset], row[offset + 1], row[offset + 2]);
    const auto selected = nearestBackgroundColor(picked, workingPalette_);
    setBackgroundColor(QColor(selected.red, selected.green, selected.blue));
}

void ImageInputController::pickColor(double normalizedX, double normalizedY, bool foreground)
{
    if (!framedSource_) return;
    normalizedX = std::clamp(normalizedX, 0.0, 1.0);
    normalizedY = std::clamp(normalizedY, 0.0, 1.0);
    const auto x = std::min<std::uint32_t>(
        framedSource_->width() - 1,
        static_cast<std::uint32_t>(normalizedX * framedSource_->width()));
    const auto y = std::min<std::uint32_t>(
        framedSource_->height() - 1,
        static_cast<std::uint32_t>(normalizedY * framedSource_->height()));
    const auto row = framedSource_->row(y);
    const std::size_t offset = static_cast<std::size_t>(x)
        * core::bytesPerPixel(framedSource_->pixelFormat());
    if (foreground) {
        setForegroundColor(QColor(row[offset], row[offset + 1], row[offset + 2]));
    } else {
        setBackgroundColor(QColor(row[offset], row[offset + 1], row[offset + 2]));
    }
}

void ImageInputController::beginSourceStroke(double normalizedX,
                                             double normalizedY,
                                             int diameter,
                                             bool eraser,
                                             bool hardEdges)
{
    if (!image_) return;
    if (sourceStrokeActive_) endSourceStroke();
    jobs_.cancelCurrent();
    debounceTimer_.stop();
    beginDrawingTransaction();
    sourceStrokeActive_ = true;
    sourceStrokeTouched_ = false;
    sourceStrokeX_ = std::clamp(normalizedX, 0.0, 1.0);
    sourceStrokeY_ = std::clamp(normalizedY, 0.0, 1.0);
    sourceStrokeDiameter_ = std::clamp(diameter, 1, 64);
    sourceStrokeEraser_ = eraser;
    sourceStrokeHardEdges_ = hardEdges;
    drawSourceStrokeSegment(sourceStrokeX_, sourceStrokeY_, sourceStrokeX_, sourceStrokeY_);
}

void ImageInputController::continueSourceStroke(double normalizedX, double normalizedY)
{
    if (!sourceStrokeActive_ || !image_) return;
    normalizedX = std::clamp(normalizedX, 0.0, 1.0);
    normalizedY = std::clamp(normalizedY, 0.0, 1.0);
    drawSourceStrokeSegment(sourceStrokeX_, sourceStrokeY_, normalizedX, normalizedY);
    sourceStrokeX_ = normalizedX;
    sourceStrokeY_ = normalizedY;
}

void ImageInputController::endSourceStroke()
{
    if (!sourceStrokeActive_) return;
    sourceStrokeActive_ = false;
    commitDrawingTransaction(sourceStrokeTouched_);
    sourceStrokeTouched_ = false;
    refreshSourcePreview();
    refreshSourceColorChoices();
    scheduleConversion();
}

void ImageInputController::drawSourceShape(double fromNormalizedX,
                                           double fromNormalizedY,
                                           double toNormalizedX,
                                           double toNormalizedY,
                                           int diameter,
                                           bool ellipse,
                                           bool hardEdges,
                                           bool fillBackground)
{
    if (!image_) return;
    if (sourceStrokeActive_) endSourceStroke();
    jobs_.cancelCurrent();
    debounceTimer_.stop();
    beginDrawingTransaction();
    sourceStrokeTouched_ = false;
    sourceStrokeDiameter_ = std::clamp(diameter, 1, 64);
    sourceStrokeEraser_ = false;
    sourceStrokeHardEdges_ = hardEdges;
    fromNormalizedX = std::clamp(fromNormalizedX, 0.0, 1.0);
    fromNormalizedY = std::clamp(fromNormalizedY, 0.0, 1.0);
    toNormalizedX = std::clamp(toNormalizedX, 0.0, 1.0);
    toNormalizedY = std::clamp(toNormalizedY, 0.0, 1.0);

    if (ellipse) {
        const double centerX = (fromNormalizedX + toNormalizedX) / 2.0;
        const double centerY = (fromNormalizedY + toNormalizedY) / 2.0;
        const double radiusX = std::abs(toNormalizedX - fromNormalizedX) / 2.0;
        const double radiusY = std::abs(toNormalizedY - fromNormalizedY) / 2.0;
        const double previewRadius = std::max(radiusX * 256.0, radiusY * 192.0);
        const int segmentCount = std::clamp(
            static_cast<int>(std::ceil(previewRadius * 1.6)), 16, 256);
        constexpr double pi = 3.14159265358979323846;
        double previousX = centerX + radiusX;
        double previousY = centerY;
        for (int segment = 1; segment <= segmentCount; ++segment) {
            const double angle = 2.0 * pi * segment / segmentCount;
            const double nextX = centerX + std::cos(angle) * radiusX;
            const double nextY = centerY + std::sin(angle) * radiusY;
            drawSourceStrokeSegment(previousX, previousY, nextX, nextY);
            previousX = nextX;
            previousY = nextY;
        }
    } else {
        drawSourceStrokeSegment(fromNormalizedX, fromNormalizedY,
                                toNormalizedX, fromNormalizedY);
        drawSourceStrokeSegment(toNormalizedX, fromNormalizedY,
                                toNormalizedX, toNormalizedY);
        drawSourceStrokeSegment(toNormalizedX, toNormalizedY,
                                fromNormalizedX, toNormalizedY);
        drawSourceStrokeSegment(fromNormalizedX, toNormalizedY,
                                fromNormalizedX, fromNormalizedY);
    }

    if (fillBackground) {
        const double inset = std::max(
            0.0, sourceStrokeDiameter_ / 2.0 - (sourceStrokeHardEdges_ ? 0.0 : 1.0));
        fillSourceShape(fromNormalizedX, fromNormalizedY,
                        toNormalizedX, toNormalizedY, ellipse, inset);
    }

    commitDrawingTransaction(sourceStrokeTouched_);
    sourceStrokeTouched_ = false;
    refreshSourcePreview();
    refreshSourceColorChoices();
    scheduleConversion();
}

void ImageInputController::beginDrawingTransaction()
{
    if (!image_) return;
    ensureDrawingLayer();
    if (!drawingLayer_) return;
    drawingBeforeImage_ = drawingLayer_;
    drawingLayer_ = std::make_shared<core::RgbImage>(*drawingLayer_);
}

void ImageInputController::commitDrawingTransaction(bool changed)
{
    if (!drawingBeforeImage_) return;
    if (changed) {
        drawingUndoStack_.push_back(std::move(drawingBeforeImage_));
        trimDrawingHistory(drawingUndoStack_);
        drawingRedoStack_.clear();
        emit drawingHistoryChanged();
    } else {
        drawingLayer_ = std::move(drawingBeforeImage_);
    }
}

void ImageInputController::undoDrawing()
{
    if (sourceStrokeActive_) endSourceStroke();
    if (!drawingLayer_ || drawingUndoStack_.empty()) return;
    drawingRedoStack_.push_back(drawingLayer_);
    trimDrawingHistory(drawingRedoStack_);
    drawingLayer_ = std::move(drawingUndoStack_.back());
    drawingUndoStack_.pop_back();
    refreshAfterDrawingHistoryChange();
}

void ImageInputController::redoDrawing()
{
    if (sourceStrokeActive_) endSourceStroke();
    if (!drawingLayer_ || drawingRedoStack_.empty()) return;
    drawingUndoStack_.push_back(drawingLayer_);
    trimDrawingHistory(drawingUndoStack_);
    drawingLayer_ = std::move(drawingRedoStack_.back());
    drawingRedoStack_.pop_back();
    refreshAfterDrawingHistoryChange();
}

void ImageInputController::clearDrawingHistory()
{
    const bool hadHistory = !drawingUndoStack_.empty() || !drawingRedoStack_.empty();
    drawingBeforeImage_.reset();
    drawingUndoStack_.clear();
    drawingRedoStack_.clear();
    if (hadHistory) emit drawingHistoryChanged();
}

void ImageInputController::refreshAfterDrawingHistoryChange()
{
    sourceStrokeActive_ = false;
    sourceStrokeTouched_ = false;
    drawingBeforeImage_.reset();
    refreshSourcePreview();
    refreshSourceColorChoices();
    scheduleConversion();
    emit drawingHistoryChanged();
}

void ImageInputController::paintDrawingPixel(int x,
                                             int y,
                                             core::RgbColor color,
                                             double coverage)
{
    if (!drawingLayer_) return;
    const int drawableWidth = powerPaintFraming_ ? 240 : 256;
    const int drawableHeight = powerPaintFraming_ ? 160 : 192;
    if (x < 0 || y < 0 || x >= drawableWidth || y >= drawableHeight) return;
    coverage = std::clamp(coverage, 0.0, 1.0);
    if (coverage <= 0.0) return;

    auto row = drawingLayer_->row(static_cast<std::uint32_t>(y));
    const std::size_t offset = static_cast<std::size_t>(x) * 4U;
    const std::array target{color.red, color.green, color.blue};
    bool changed = false;
    if (coverage >= 1.0) {
        for (std::size_t channel = 0; channel < target.size(); ++channel) {
            changed |= row[offset + channel] != target[channel];
            row[offset + channel] = target[channel];
        }
        changed |= row[offset + 3U] != 255U;
        row[offset + 3U] = 255U;
    } else {
        const double previousAlpha = row[offset + 3U] / 255.0;
        const double outputAlpha = coverage
            + previousAlpha * (1.0 - coverage);
        for (std::size_t channel = 0; channel < target.size(); ++channel) {
            const double premultiplied = target[channel] * coverage
                + row[offset + channel] * previousAlpha * (1.0 - coverage);
            const auto value = static_cast<std::uint8_t>(std::clamp(
                static_cast<int>(std::lround(
                    outputAlpha > 0.0 ? premultiplied / outputAlpha : 0.0)),
                0, 255));
            changed |= row[offset + channel] != value;
            row[offset + channel] = value;
        }
        const auto alpha = static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(std::lround(outputAlpha * 255.0)), 0, 255));
        changed |= row[offset + 3U] != alpha;
        row[offset + 3U] = alpha;
    }
    sourceStrokeTouched_ |= changed;
}

void ImageInputController::fillSourceShape(double fromNormalizedX,
                                           double fromNormalizedY,
                                           double toNormalizedX,
                                           double toNormalizedY,
                                           bool ellipse,
                                           double inset)
{
    constexpr double previewWidth = 256.0;
    constexpr double previewHeight = 192.0;
    const double fromX = fromNormalizedX * previewWidth;
    const double fromY = fromNormalizedY * previewHeight;
    const double toX = toNormalizedX * previewWidth;
    const double toY = toNormalizedY * previewHeight;
    const double left = std::min(fromX, toX);
    const double right = std::max(fromX, toX);
    const double top = std::min(fromY, toY);
    const double bottom = std::max(fromY, toY);

    if (ellipse) {
        const double centerX = (left + right) / 2.0;
        const double centerY = (top + bottom) / 2.0;
        const double radiusX = (right - left) / 2.0 - inset;
        const double radiusY = (bottom - top) / 2.0 - inset;
        if (radiusX <= 0.0 || radiusY <= 0.0) return;
        const int firstY = static_cast<int>(std::floor(centerY - radiusY));
        const int lastY = static_cast<int>(std::ceil(centerY + radiusY));
        for (int y = firstY; y <= lastY; ++y) {
            const double pixelY = y + 0.5;
            const double normalizedDistance = (pixelY - centerY) / radiusY;
            if (std::abs(normalizedDistance) > 1.0) continue;
            const double halfWidth = radiusX
                * std::sqrt(std::max(0.0,
                                     1.0 - normalizedDistance * normalizedDistance));
            const int firstX = static_cast<int>(std::floor(centerX - halfWidth));
            const int lastX = static_cast<int>(std::ceil(centerX + halfWidth));
            for (int x = firstX; x <= lastX; ++x) {
                if (x + 0.5 >= centerX - halfWidth
                    && x + 0.5 <= centerX + halfWidth) {
                    paintDrawingPixel(x, y, backgroundColor_, 1.0);
                }
            }
        }
        return;
    }

    const double innerLeft = left + inset;
    const double innerRight = right - inset;
    const int firstY = static_cast<int>(std::floor(top + inset));
    const int lastY = static_cast<int>(std::ceil(bottom - inset));
    if (innerLeft > innerRight || firstY > lastY) return;
    for (int y = firstY; y <= lastY; ++y) {
        if (y + 0.5 < top + inset || y + 0.5 > bottom - inset) continue;
        const int firstX = static_cast<int>(std::floor(innerLeft));
        const int lastX = static_cast<int>(std::ceil(innerRight));
        for (int x = firstX; x <= lastX; ++x) {
            if (x + 0.5 >= innerLeft && x + 0.5 <= innerRight) {
                paintDrawingPixel(x, y, backgroundColor_, 1.0);
            }
        }
    }
}

void ImageInputController::drawSourceStrokeSegment(double fromNormalizedX,
                                                   double fromNormalizedY,
                                                   double toNormalizedX,
                                                   double toNormalizedY)
{
    if (!drawingLayer_) return;
    constexpr double previewWidth = 256.0;
    constexpr double previewHeight = 192.0;
    const auto coordinate = [this](double normalized, double extent) {
        const double value = std::clamp(normalized, 0.0, 1.0) * extent;
        if (!sourceStrokeHardEdges_) return value;
        return std::min(extent - 0.5, std::floor(value) + 0.5);
    };
    const double fromX = coordinate(fromNormalizedX, previewWidth);
    const double fromY = coordinate(fromNormalizedY, previewHeight);
    const double toX = coordinate(toNormalizedX, previewWidth);
    const double toY = coordinate(toNormalizedY, previewHeight);
    const double brushRadius = sourceStrokeDiameter_ / 2.0;
    const double feather = sourceStrokeHardEdges_ ? 0.0 : 1.0;
    const double extent = brushRadius + feather;
    const auto paintColor = sourceStrokeEraser_ ? backgroundColor_ : foregroundColor_;
    const double deltaX = toX - fromX;
    const double deltaY = toY - fromY;
    const double lengthSquared = deltaX * deltaX + deltaY * deltaY;
    const int minimumX = static_cast<int>(std::floor(std::min(fromX, toX) - extent));
    const int maximumX = static_cast<int>(std::ceil(std::max(fromX, toX) + extent));
    const int minimumY = static_cast<int>(std::floor(std::min(fromY, toY) - extent));
    const int maximumY = static_cast<int>(std::ceil(std::max(fromY, toY) + extent));
    bool coveredPixel = false;
    for (int y = minimumY; y <= maximumY; ++y) {
        for (int x = minimumX; x <= maximumX; ++x) {
            double nearestAmount = 0.0;
            if (lengthSquared > 0.0) {
                nearestAmount = std::clamp(
                    ((x + 0.5 - fromX) * deltaX + (y + 0.5 - fromY) * deltaY)
                        / lengthSquared,
                    0.0, 1.0);
            }
            const double nearestX = fromX + nearestAmount * deltaX;
            const double nearestY = fromY + nearestAmount * deltaY;
            const double distance = std::hypot(x + 0.5 - nearestX,
                                               y + 0.5 - nearestY);
            const double coverage = sourceStrokeHardEdges_
                ? (distance <= brushRadius ? 1.0 : 0.0)
                : std::clamp(brushRadius + 1.0 - distance, 0.0, 1.0);
            if (coverage <= 0.0) continue;
            coveredPixel = true;
            paintDrawingPixel(x, y, paintColor, coverage);
        }
    }
    if (!coveredPixel) {
        paintDrawingPixel(static_cast<int>(std::floor(fromX)),
                          static_cast<int>(std::floor(fromY)),
                          paintColor, 1.0);
    }
}

ImageInputController::SettingsSnapshot ImageInputController::snapshot() const
{
    return {settings_,
            scalingFilter_,
            fillMode_,
            horizontalOffset_,
            verticalOffset_,
            foregroundColor_,
            backgroundColor_,
            workingPalette_,
            powerPaintFraming_};
}

void ImageInputController::recordUndo()
{
    if (applyingSnapshot_ || undoCoalesceTimer_.isActive()) {
        undoCoalesceTimer_.start();
        return;
    }
    undoStack_.push_back(snapshot());
    if (undoStack_.size() > 32U) undoStack_.erase(undoStack_.begin());
    undoCoalesceTimer_.start();
}

void ImageInputController::applySnapshot(const SettingsSnapshot& value)
{
    applyingSnapshot_ = true;
    settings_ = value.settings;
    scalingFilter_ = value.scalingFilter;
    fillMode_ = value.fillMode;
    horizontalOffset_ = value.horizontalOffset;
    verticalOffset_ = value.verticalOffset;
    foregroundColor_ = value.foregroundColor;
    backgroundColor_ = value.backgroundColor;
    workingPalette_ = value.workingPalette;
    powerPaintFraming_ = value.powerPaintFraming;
    applyingSnapshot_ = false;
    saveSettings();
    refreshSourcePreview();
    refreshSourceColorChoices();
    emit settingsChanged();
    scheduleConversion();
}

void ImageInputController::undoSettings()
{
    if (undoStack_.empty()) return;
    undoCoalesceTimer_.stop();
    const auto value = undoStack_.back();
    undoStack_.pop_back();
    applySnapshot(value);
}

void ImageInputController::resetSettings()
{
    recordUndo();
    SettingsSnapshot defaults;
    defaults.settings = core::ConversionSettings{};
    defaults.scalingFilter = core::ScalingFilter::Bilinear;
    defaults.fillMode = core::ImageFillMode::Fit;
    defaults.foregroundColor = {255, 255, 255};
    defaults.backgroundColor = {0, 0, 0};
    defaults.workingPalette = defaultWorkingColors();
    defaults.powerPaintFraming = false;
    applySnapshot(defaults);
}

void ImageInputController::applyPreset(int presetIndex)
{
    recordUndo();
    auto value = snapshot();
    switch (presetIndex) {
    case 1: // Pixel art
        value.settings.dither = core::DitherMode::None;
        value.settings.stretchHistogram = false;
        value.settings.maximumColorShiftPercent = 0.0;
        value.scalingFilter = core::ScalingFilter::None;
        break;
    case 2: // Smooth photo
        value.settings.dither = core::DitherMode::Atkinson;
        value.settings.stretchHistogram = true;
        value.settings.maximumColorShiftPercent = 2.0;
        value.scalingFilter = core::ScalingFilter::Blackman;
        break;
    case 3: // Ordered retro
        value.settings.dither = core::DitherMode::Ordered;
        value.settings.stretchHistogram = false;
        value.settings.orderedDitherBrightness = 0;
        value.scalingFilter = core::ScalingFilter::Bilinear;
        break;
    default: // Balanced
        value.settings = core::ConversionSettings{};
        value.scalingFilter = core::ScalingFilter::Bilinear;
        break;
    }
    if (const auto configuration = core::ditherConfiguration(value.settings.dither)) {
        value.settings.errorDistribution = configuration->kernel;
    }
    applySnapshot(value);
}

QJsonObject ImageInputController::recipeSettings() const
{
    QJsonArray palette;
    for (const auto& color : workingPalette_) palette.push_back(colorName(color));
    return {
        {QStringLiteral("mode"), conversionMode()},
        {QStringLiteral("dither"), ditherMode()},
        {QStringLiteral("scalingFilter"), scalingFilter()},
        {QStringLiteral("fillMode"), fillMode()},
        {QStringLiteral("perceptualColorMatching"), perceptualColorMatching()},
        {QStringLiteral("perceptualRedWeight"), perceptualRedWeight()},
        {QStringLiteral("perceptualGreenWeight"), perceptualGreenWeight()},
        {QStringLiteral("perceptualBlueWeight"), perceptualBlueWeight()},
        {QStringLiteral("stretchHistogram"), stretchHistogram()},
        {QStringLiteral("maximumColorShift"), maximumColorShift()},
        {QStringLiteral("gamma"), gamma()},
        {QStringLiteral("lumaEmphasis"), lumaEmphasis()},
        {QStringLiteral("maximumMulticolorDifference"), maximumMulticolorDifference()},
        {QStringLiteral("orderedBrightness"), orderedBrightness()},
        {QStringLiteral("errorAccumulation"), errorAccumulationMode()},
        {QStringLiteral("orderedDitherMapSize"), orderedDitherMapSize()},
        {QStringLiteral("errorDownLeft"), errorDownLeft()},
        {QStringLiteral("errorDown"), errorDown()},
        {QStringLiteral("errorDownRight"), errorDownRight()},
        {QStringLiteral("errorRight"), errorRight()},
        {QStringLiteral("errorFarRight"), errorFarRight()},
        {QStringLiteral("errorDownTwo"), errorDownTwo()},
        {QStringLiteral("paletteSelection"), paletteSelectionMode()},
        {QStringLiteral("scanlineStaticColorCount"), scanlineStaticColorCount()},
        {QStringLiteral("scanlineRegion1"), scanlineRegion1()},
        {QStringLiteral("scanlineRegion2"), scanlineRegion2()},
        {QStringLiteral("scanlineRegion3"), scanlineRegion3()},
        {QStringLiteral("horizontalOffset"), horizontalOffset()},
        {QStringLiteral("verticalOffset"), verticalOffset()},
        {QStringLiteral("foregroundColor"), foregroundColor().name()},
        {QStringLiteral("backgroundColor"), backgroundColor().name()},
        {QStringLiteral("workingPalette"), palette},
        {QStringLiteral("powerPaintFraming"), powerPaintFraming()},
        {QStringLiteral("autoUpdate"), autoUpdate()},
        {QStringLiteral("livePreview"), livePreview()},
        {QStringLiteral("exportFormat"), exportFormat()},
    };
}

bool ImageInputController::applyRecipeSettings(const QJsonObject& object, QString* error)
{
    if (object.isEmpty()) {
        if (error) *error = QStringLiteral("The recipe does not contain conversion settings.");
        return false;
    }
    SettingsSnapshot value = snapshot();
    value.settings.mode = static_cast<core::ConversionMode>(
        std::clamp(object.value(QStringLiteral("mode")).toInt(conversionMode()), 0, 8));
    value.settings.dither = static_cast<core::DitherMode>(
        std::clamp(object.value(QStringLiteral("dither")).toInt(ditherMode()), 0, 7));
    value.scalingFilter = static_cast<core::ScalingFilter>(
        std::clamp(object.value(QStringLiteral("scalingFilter")).toInt(scalingFilter()), 0, 5));
    value.fillMode = static_cast<core::ImageFillMode>(
        std::clamp(object.value(QStringLiteral("fillMode")).toInt(fillMode()), 0, 3));
    value.settings.perceptualColorMatching = object.value(
        QStringLiteral("perceptualColorMatching")).toBool(perceptualColorMatching());
    value.settings.perceptualRedWeight = std::clamp(
        object.value(QStringLiteral("perceptualRedWeight")).toInt(perceptualRedWeight()) / 100.0,
        0.0, 1.0);
    value.settings.perceptualGreenWeight = std::clamp(
        object.value(QStringLiteral("perceptualGreenWeight")).toInt(perceptualGreenWeight()) / 100.0,
        0.0, 1.0);
    value.settings.perceptualBlueWeight = std::clamp(
        object.value(QStringLiteral("perceptualBlueWeight")).toInt(perceptualBlueWeight()) / 100.0,
        0.0, 1.0);
    value.settings.stretchHistogram = object.value(
        QStringLiteral("stretchHistogram")).toBool(stretchHistogram());
    value.settings.maximumColorShiftPercent = std::clamp(
        object.value(QStringLiteral("maximumColorShift")).toDouble(maximumColorShift()),
        0.0, 100.0);
    value.settings.gamma = std::clamp(
        object.value(QStringLiteral("gamma")).toDouble(gamma()), 0.1, 5.0);
    value.settings.lumaEmphasis = std::clamp(
        object.value(QStringLiteral("lumaEmphasis")).toDouble(lumaEmphasis()), 0.0, 10.0);
    value.settings.maximumMulticolorDifferencePercent = std::clamp(
        object.value(QStringLiteral("maximumMulticolorDifference"))
            .toInt(maximumMulticolorDifference()), 0, 100);
    value.settings.orderedDitherBrightness = std::clamp(
        object.value(QStringLiteral("orderedBrightness")).toInt(orderedBrightness()), 0, 16);
    value.settings.errorAccumulation = static_cast<core::ErrorAccumulationMode>(
        std::clamp(object.value(QStringLiteral("errorAccumulation"))
                       .toInt(errorAccumulationMode()), 0, 1));
    value.settings.orderedDitherMapSize =
        object.value(QStringLiteral("orderedDitherMapSize")).toInt(orderedDitherMapSize()) == 4
        ? core::OrderedDitherMapSize::FourByFour
        : core::OrderedDitherMapSize::TwoByTwo;
    value.settings.errorDistribution = {
        static_cast<std::uint8_t>(std::clamp(
            object.value(QStringLiteral("errorDownLeft")).toInt(errorDownLeft()), 0, 16)),
        static_cast<std::uint8_t>(std::clamp(
            object.value(QStringLiteral("errorDown")).toInt(errorDown()), 0, 16)),
        static_cast<std::uint8_t>(std::clamp(
            object.value(QStringLiteral("errorDownRight")).toInt(errorDownRight()), 0, 16)),
        static_cast<std::uint8_t>(std::clamp(
            object.value(QStringLiteral("errorRight")).toInt(errorRight()), 0, 16)),
        static_cast<std::uint8_t>(std::clamp(
            object.value(QStringLiteral("errorFarRight")).toInt(errorFarRight()), 0, 16)),
        static_cast<std::uint8_t>(std::clamp(
            object.value(QStringLiteral("errorDownTwo")).toInt(errorDownTwo()), 0, 16)),
    };
    value.settings.paletteSelection = static_cast<core::PaletteSelectionMode>(
        std::clamp(object.value(QStringLiteral("paletteSelection"))
                       .toInt(paletteSelectionMode()), 0, 1));
    value.settings.scanlineStaticColorCount = std::clamp(
        object.value(QStringLiteral("scanlineStaticColorCount"))
            .toInt(scanlineStaticColorCount()), 0, 14);
    value.settings.scanlineRegion1 = object.value(
        QStringLiteral("scanlineRegion1")).toBool(scanlineRegion1());
    value.settings.scanlineRegion2 = object.value(
        QStringLiteral("scanlineRegion2")).toBool(scanlineRegion2());
    value.settings.scanlineRegion3 = object.value(
        QStringLiteral("scanlineRegion3")).toBool(scanlineRegion3());
    value.horizontalOffset = std::clamp(
        object.value(QStringLiteral("horizontalOffset")).toInt(horizontalOffset()), -256, 256);
    value.verticalOffset = std::clamp(
        object.value(QStringLiteral("verticalOffset")).toInt(verticalOffset()), -192, 192);
    const QColor foreground(object.value(QStringLiteral("foregroundColor"))
                                .toString(foregroundColor().name()));
    const QColor background(object.value(QStringLiteral("backgroundColor"))
                                .toString(backgroundColor().name()));
    if (!foreground.isValid() || !background.isValid()) {
        if (error) *error = QStringLiteral("The recipe contains an invalid drawing color.");
        return false;
    }
    value.foregroundColor = {static_cast<std::uint8_t>(foreground.red()),
                             static_cast<std::uint8_t>(foreground.green()),
                             static_cast<std::uint8_t>(foreground.blue())};
    value.backgroundColor = {static_cast<std::uint8_t>(background.red()),
                             static_cast<std::uint8_t>(background.green()),
                             static_cast<std::uint8_t>(background.blue())};
    const QJsonArray savedPalette = object.value(QStringLiteral("workingPalette")).toArray();
    if (!savedPalette.isEmpty()) {
        if (savedPalette.size() != 15) {
            if (error) *error = QStringLiteral("The working palette must contain 15 colors.");
            return false;
        }
        std::vector<core::RgbColor> colors;
        colors.reserve(15);
        for (const auto savedColor : savedPalette) {
            const QColor color(savedColor.toString());
            if (!color.isValid()) {
                if (error) *error = QStringLiteral("The working palette contains an invalid color.");
                return false;
            }
            colors.push_back({static_cast<std::uint8_t>(color.red()),
                              static_cast<std::uint8_t>(color.green()),
                              static_cast<std::uint8_t>(color.blue())});
        }
        value.workingPalette = std::move(colors);
    }
    value.powerPaintFraming = object.value(
        QStringLiteral("powerPaintFraming")).toBool(powerPaintFraming());

    autoUpdate_ = object.value(QStringLiteral("autoUpdate")).toBool(autoUpdate_);
    livePreview_ = object.value(QStringLiteral("livePreview")).toBool(livePreview_);
    exportFormat_ = std::clamp(
        object.value(QStringLiteral("exportFormat")).toInt(exportFormat_), 0, 8);
    undoStack_.clear();
    applySnapshot(value);
    emit exportChanged();
    if (error) error->clear();
    return true;
}

void ImageInputController::settingsWereChanged(bool sourceTransformChanged)
{
    saveSettings();
    if (sourceTransformChanged) {
        refreshSourcePreview();
        refreshSourceColorChoices();
    }
    emit settingsChanged();
    scheduleConversion();
}

void ImageInputController::loadSettings()
{
    QSettings persisted;
    persisted.beginGroup(QStringLiteral("conversion"));
    settings_.mode = static_cast<core::ConversionMode>(
        std::clamp(persisted.value(QStringLiteral("mode"), 0).toInt(), 0, 8));
    settings_.dither = static_cast<core::DitherMode>(
        std::clamp(persisted.value(QStringLiteral("dither"), 2).toInt(), 0, 7));
    const auto defaultDithering = core::ditherConfiguration(settings_.dither);
    const core::ErrorDistributionKernel defaultKernel = defaultDithering
        ? defaultDithering->kernel
        : core::ErrorDistributionKernel{2, 2, 2, 2, 1, 1};
    settings_.errorDistribution = {
        static_cast<std::uint8_t>(std::clamp(
            persisted.value(QStringLiteral("errorDownLeft"), defaultKernel.downLeft).toInt(),
            0, 16)),
        static_cast<std::uint8_t>(std::clamp(
            persisted.value(QStringLiteral("errorDown"), defaultKernel.down).toInt(), 0, 16)),
        static_cast<std::uint8_t>(std::clamp(
            persisted.value(QStringLiteral("errorDownRight"), defaultKernel.downRight).toInt(),
            0, 16)),
        static_cast<std::uint8_t>(std::clamp(
            persisted.value(QStringLiteral("errorRight"), defaultKernel.right).toInt(), 0, 16)),
        static_cast<std::uint8_t>(std::clamp(
            persisted.value(QStringLiteral("errorFarRight"), defaultKernel.farRight).toInt(),
            0, 16)),
        static_cast<std::uint8_t>(std::clamp(
            persisted.value(QStringLiteral("errorDownTwo"), defaultKernel.downTwo).toInt(),
            0, 16)),
    };
    scalingFilter_ = static_cast<core::ScalingFilter>(
        std::clamp(persisted.value(QStringLiteral("scalingFilter"), 4).toInt(), 0, 5));
    fillMode_ = static_cast<core::ImageFillMode>(
        std::clamp(persisted.value(QStringLiteral("fillMode"), 0).toInt(), 0, 3));
    settings_.perceptualColorMatching =
        persisted.value(QStringLiteral("perceptual"), false).toBool();
    settings_.perceptualRedWeight = std::clamp(
        persisted.value(QStringLiteral("perceptualRedWeight"), 0.30).toDouble(), 0.0, 1.0);
    settings_.perceptualGreenWeight = std::clamp(
        persisted.value(QStringLiteral("perceptualGreenWeight"), 0.52).toDouble(), 0.0, 1.0);
    settings_.perceptualBlueWeight = std::clamp(
        persisted.value(QStringLiteral("perceptualBlueWeight"), 0.18).toDouble(), 0.0, 1.0);
    settings_.stretchHistogram = persisted.value(QStringLiteral("histogram"), false).toBool();
    settings_.maximumColorShiftPercent =
        std::clamp(persisted.value(QStringLiteral("colorShift"), 1.0).toDouble(), 0.0, 100.0);
    settings_.gamma =
        std::clamp(persisted.value(QStringLiteral("gamma"), 1.0).toDouble(), 0.1, 5.0);
    settings_.lumaEmphasis =
        std::clamp(persisted.value(QStringLiteral("luma"), 1.2).toDouble(), 0.0, 10.0);
    settings_.maximumMulticolorDifferencePercent =
        std::clamp(persisted.value(QStringLiteral("flicker"), 95).toInt(), 0, 100);
    settings_.orderedDitherBrightness =
        std::clamp(persisted.value(QStringLiteral("orderedBrightness"), 0).toInt(), 0, 16);
    settings_.errorAccumulation = static_cast<core::ErrorAccumulationMode>(
        std::clamp(persisted.value(QStringLiteral("errorAccumulation"), 1).toInt(), 0, 1));
    const int persistedOrderedMapSize =
        persisted.value(QStringLiteral("orderedDitherMapSize"), 2).toInt();
    settings_.orderedDitherMapSize = persistedOrderedMapSize == 4
        ? core::OrderedDitherMapSize::FourByFour
        : core::OrderedDitherMapSize::TwoByTwo;
    settings_.paletteSelection = static_cast<core::PaletteSelectionMode>(
        std::clamp(persisted.value(QStringLiteral("paletteSelection"), 0).toInt(), 0, 1));
    settings_.scanlineStaticColorCount = std::clamp(
        persisted.value(QStringLiteral("scanlineStaticColorCount"), 0).toInt(), 0, 14);
    settings_.scanlineRegion1 = persisted.value(QStringLiteral("scanlineRegion1"), true).toBool();
    settings_.scanlineRegion2 = persisted.value(QStringLiteral("scanlineRegion2"), true).toBool();
    settings_.scanlineRegion3 = persisted.value(QStringLiteral("scanlineRegion3"), true).toBool();
    horizontalOffset_ =
        std::clamp(persisted.value(QStringLiteral("horizontalOffset"), 0).toInt(), -256, 256);
    verticalOffset_ =
        std::clamp(persisted.value(QStringLiteral("verticalOffset"), 0).toInt(), -192, 192);
    const QStringList storedPalette =
        persisted.value(QStringLiteral("workingPalette")).toStringList();
    if (storedPalette.size() == 15) {
        std::vector<core::RgbColor> colors;
        colors.reserve(15);
        for (const QString& name : storedPalette) {
            const QColor color(name);
            if (!color.isValid()) {
                colors.clear();
                break;
            }
            colors.push_back({static_cast<std::uint8_t>(color.red()),
                              static_cast<std::uint8_t>(color.green()),
                              static_cast<std::uint8_t>(color.blue())});
        }
        if (colors.size() == 15U) workingPalette_ = std::move(colors);
    }
    const QColor persistedBackground(
        persisted.value(QStringLiteral("backgroundColor"), QStringLiteral("#000000"))
            .toString());
    const QColor persistedForeground(
        persisted.value(QStringLiteral("foregroundColor"), QStringLiteral("#FFFFFF"))
            .toString());
    foregroundColor_ = persistedForeground.isValid()
        ? core::RgbColor{static_cast<std::uint8_t>(persistedForeground.red()),
                         static_cast<std::uint8_t>(persistedForeground.green()),
                         static_cast<std::uint8_t>(persistedForeground.blue())}
        : core::RgbColor{255, 255, 255};
    const QColor loadedBackground = persistedBackground.isValid()
        ? persistedBackground : QColor(Qt::black);
    backgroundColor_ = {static_cast<std::uint8_t>(loadedBackground.red()),
                        static_cast<std::uint8_t>(loadedBackground.green()),
                        static_cast<std::uint8_t>(loadedBackground.blue())};
    powerPaintFraming_ = persisted.value(QStringLiteral("powerPaintFraming"), false).toBool();
    autoUpdate_ = persisted.value(QStringLiteral("autoUpdate"), true).toBool();
    livePreview_ = persisted.value(QStringLiteral("livePreview"), false).toBool();
    exportFormat_ = std::clamp(persisted.value(QStringLiteral("exportFormat"), 0).toInt(), 0, 8);
    persisted.endGroup();
}

void ImageInputController::saveSettings() const
{
    QSettings persisted;
    persisted.beginGroup(QStringLiteral("conversion"));
    persisted.setValue(QStringLiteral("mode"), conversionMode());
    persisted.setValue(QStringLiteral("dither"), ditherMode());
    persisted.setValue(QStringLiteral("scalingFilter"), scalingFilter());
    persisted.setValue(QStringLiteral("fillMode"), fillMode());
    persisted.setValue(QStringLiteral("perceptual"), perceptualColorMatching());
    persisted.setValue(QStringLiteral("perceptualRedWeight"), settings_.perceptualRedWeight);
    persisted.setValue(QStringLiteral("perceptualGreenWeight"), settings_.perceptualGreenWeight);
    persisted.setValue(QStringLiteral("perceptualBlueWeight"), settings_.perceptualBlueWeight);
    persisted.setValue(QStringLiteral("histogram"), stretchHistogram());
    persisted.setValue(QStringLiteral("colorShift"), maximumColorShift());
    persisted.setValue(QStringLiteral("gamma"), gamma());
    persisted.setValue(QStringLiteral("luma"), lumaEmphasis());
    persisted.setValue(QStringLiteral("flicker"), maximumMulticolorDifference());
    persisted.setValue(QStringLiteral("orderedBrightness"), orderedBrightness());
    persisted.setValue(QStringLiteral("errorAccumulation"), errorAccumulationMode());
    persisted.setValue(QStringLiteral("orderedDitherMapSize"), orderedDitherMapSize());
    persisted.setValue(QStringLiteral("errorDownLeft"), errorDownLeft());
    persisted.setValue(QStringLiteral("errorDown"), errorDown());
    persisted.setValue(QStringLiteral("errorDownRight"), errorDownRight());
    persisted.setValue(QStringLiteral("errorRight"), errorRight());
    persisted.setValue(QStringLiteral("errorFarRight"), errorFarRight());
    persisted.setValue(QStringLiteral("errorDownTwo"), errorDownTwo());
    persisted.setValue(QStringLiteral("paletteSelection"), paletteSelectionMode());
    persisted.setValue(QStringLiteral("scanlineStaticColorCount"), scanlineStaticColorCount());
    persisted.setValue(QStringLiteral("scanlineRegion1"), scanlineRegion1());
    persisted.setValue(QStringLiteral("scanlineRegion2"), scanlineRegion2());
    persisted.setValue(QStringLiteral("scanlineRegion3"), scanlineRegion3());
    persisted.setValue(QStringLiteral("horizontalOffset"), horizontalOffset());
    persisted.setValue(QStringLiteral("verticalOffset"), verticalOffset());
    persisted.setValue(QStringLiteral("foregroundColor"), foregroundColor().name());
    persisted.setValue(QStringLiteral("backgroundColor"), backgroundColor().name());
    QStringList paletteNames;
    for (const auto& color : workingPalette_) paletteNames.push_back(colorName(color));
    persisted.setValue(QStringLiteral("workingPalette"), paletteNames);
    persisted.setValue(QStringLiteral("powerPaintFraming"), powerPaintFraming_);
    persisted.setValue(QStringLiteral("autoUpdate"), autoUpdate_);
    persisted.setValue(QStringLiteral("livePreview"), livePreview_);
    persisted.setValue(QStringLiteral("exportFormat"), exportFormat_);
    persisted.endGroup();
    persisted.sync();
}

void ImageInputController::setConversionMode(int value)
{
    value = std::clamp(value, 0, 8);
    if (value == conversionMode()) return;
    recordUndo();
    settings_.mode = static_cast<core::ConversionMode>(value);
    settingsWereChanged();
}

void ImageInputController::setDitherMode(int value)
{
    value = std::clamp(value, 0, 7);
    if (value == ditherMode()) return;
    recordUndo();
    settings_.dither = static_cast<core::DitherMode>(value);
    if (settings_.dither != core::DitherMode::Custom) {
        const auto configuration = core::ditherConfiguration(settings_.dither);
        if (configuration) settings_.errorDistribution = configuration->kernel;
    }
    settingsWereChanged();
}

void ImageInputController::setScalingFilter(int value)
{
    value = std::clamp(value, 0, 5);
    if (value == scalingFilter()) return;
    recordUndo();
    scalingFilter_ = static_cast<core::ScalingFilter>(value);
    settingsWereChanged(true);
}

void ImageInputController::setFillMode(int value)
{
    value = std::clamp(value, 0, 3);
    if (value == fillMode()) return;
    recordUndo();
    fillMode_ = static_cast<core::ImageFillMode>(value);
    settingsWereChanged(true);
}

void ImageInputController::setExportFormat(int value)
{
    value = std::clamp(value, 0, 8);
    if (value == exportFormat_) return;
    exportFormat_ = value;
    saveSettings();
    updateExportSummary();
}

void ImageInputController::setPerceptualColorMatching(bool value)
{
    if (value == settings_.perceptualColorMatching) return;
    recordUndo();
    settings_.perceptualColorMatching = value;
    settingsWereChanged();
}

void ImageInputController::setStretchHistogram(bool value)
{
    if (value == settings_.stretchHistogram) return;
    recordUndo();
    settings_.stretchHistogram = value;
    settingsWereChanged();
}

void ImageInputController::setPerceptualRedWeight(int value)
{
    value = std::clamp(value, 0, 100);
    if (value == perceptualRedWeight()) return;
    recordUndo();
    settings_.perceptualRedWeight = static_cast<double>(value) / 100.0;
    settingsWereChanged();
}

void ImageInputController::setPerceptualGreenWeight(int value)
{
    value = std::clamp(value, 0, 100);
    if (value == perceptualGreenWeight()) return;
    recordUndo();
    settings_.perceptualGreenWeight = static_cast<double>(value) / 100.0;
    settingsWereChanged();
}

void ImageInputController::setPerceptualBlueWeight(int value)
{
    value = std::clamp(value, 0, 100);
    if (value == perceptualBlueWeight()) return;
    recordUndo();
    settings_.perceptualBlueWeight = static_cast<double>(value) / 100.0;
    settingsWereChanged();
}

void ImageInputController::restorePerceptualWeights()
{
    if (perceptualRedWeight() == 30 && perceptualGreenWeight() == 52
        && perceptualBlueWeight() == 18) return;
    recordUndo();
    settings_.perceptualRedWeight = 0.30;
    settings_.perceptualGreenWeight = 0.52;
    settings_.perceptualBlueWeight = 0.18;
    settingsWereChanged();
}

void ImageInputController::setMaximumColorShift(double value)
{
    value = std::clamp(value, 0.0, 100.0);
    if (qFuzzyCompare(value + 1.0, settings_.maximumColorShiftPercent + 1.0)) return;
    recordUndo();
    settings_.maximumColorShiftPercent = value;
    settingsWereChanged();
}

void ImageInputController::setGamma(double value)
{
    value = std::clamp(value, 0.1, 5.0);
    if (qFuzzyCompare(value, settings_.gamma)) return;
    recordUndo();
    settings_.gamma = value;
    settingsWereChanged();
}

void ImageInputController::setLumaEmphasis(double value)
{
    value = std::clamp(value, 0.0, 10.0);
    if (qFuzzyCompare(value + 1.0, settings_.lumaEmphasis + 1.0)) return;
    recordUndo();
    settings_.lumaEmphasis = value;
    settingsWereChanged();
}

void ImageInputController::setMaximumMulticolorDifference(int value)
{
    value = std::clamp(value, 0, 100);
    if (value == settings_.maximumMulticolorDifferencePercent) return;
    recordUndo();
    settings_.maximumMulticolorDifferencePercent = value;
    settingsWereChanged();
}

void ImageInputController::setOrderedBrightness(int value)
{
    value = std::clamp(value, 0, 16);
    if (value == settings_.orderedDitherBrightness) return;
    recordUndo();
    settings_.orderedDitherBrightness = value;
    settingsWereChanged();
}

void ImageInputController::setErrorAccumulationMode(int value)
{
    value = std::clamp(value, 0, 1);
    if (value == errorAccumulationMode()) return;
    recordUndo();
    settings_.errorAccumulation = static_cast<core::ErrorAccumulationMode>(value);
    settingsWereChanged();
}

void ImageInputController::setOrderedDitherMapSize(int value)
{
    const auto size = value == 4 ? core::OrderedDitherMapSize::FourByFour
                                 : core::OrderedDitherMapSize::TwoByTwo;
    if (size == settings_.orderedDitherMapSize) return;
    recordUndo();
    settings_.orderedDitherMapSize = size;
    settingsWereChanged();
}

void ImageInputController::setErrorDistributionWeight(int index, int value)
{
    value = std::clamp(value, 0, 16);
    std::uint8_t* weight = nullptr;
    switch (index) {
    case 0: weight = &settings_.errorDistribution.downLeft; break;
    case 1: weight = &settings_.errorDistribution.down; break;
    case 2: weight = &settings_.errorDistribution.downRight; break;
    case 3: weight = &settings_.errorDistribution.right; break;
    case 4: weight = &settings_.errorDistribution.farRight; break;
    case 5: weight = &settings_.errorDistribution.downTwo; break;
    default: return;
    }
    if (*weight == value && settings_.dither == core::DitherMode::Custom) return;
    recordUndo();
    *weight = static_cast<std::uint8_t>(value);
    settings_.dither = core::DitherMode::Custom;
    settingsWereChanged();
}

void ImageInputController::setErrorDownLeft(int value)
{
    setErrorDistributionWeight(0, value);
}

void ImageInputController::setErrorDown(int value)
{
    setErrorDistributionWeight(1, value);
}

void ImageInputController::setErrorDownRight(int value)
{
    setErrorDistributionWeight(2, value);
}

void ImageInputController::setErrorRight(int value)
{
    setErrorDistributionWeight(3, value);
}

void ImageInputController::setErrorFarRight(int value)
{
    setErrorDistributionWeight(4, value);
}

void ImageInputController::setErrorDownTwo(int value)
{
    setErrorDistributionWeight(5, value);
}

void ImageInputController::setPaletteSelectionMode(int value)
{
    value = std::clamp(value, 0, 1);
    if (value == paletteSelectionMode()) return;
    recordUndo();
    settings_.paletteSelection = static_cast<core::PaletteSelectionMode>(value);
    settingsWereChanged();
}

void ImageInputController::setScanlineStaticColorCount(int value)
{
    value = std::clamp(value, 0, 14);
    if (value == scanlineStaticColorCount()) return;
    recordUndo();
    settings_.scanlineStaticColorCount = value;
    if (value > 0 && !settings_.scanlineRegion1 && !settings_.scanlineRegion2
        && !settings_.scanlineRegion3) {
        settings_.scanlineRegion1 = true;
    }
    settingsWereChanged();
}

void ImageInputController::setScanlineRegion1(bool value)
{
    if (value == settings_.scanlineRegion1) return;
    recordUndo();
    settings_.scanlineRegion1 = value;
    settingsWereChanged();
}

void ImageInputController::setScanlineRegion2(bool value)
{
    if (value == settings_.scanlineRegion2) return;
    recordUndo();
    settings_.scanlineRegion2 = value;
    settingsWereChanged();
}

void ImageInputController::setScanlineRegion3(bool value)
{
    if (value == settings_.scanlineRegion3) return;
    recordUndo();
    settings_.scanlineRegion3 = value;
    settingsWereChanged();
}

void ImageInputController::setPowerPaintFraming(bool value)
{
    if (value == powerPaintFraming_) return;
    recordUndo();
    powerPaintFraming_ = value;
    settingsWereChanged(true);
}

void ImageInputController::setHorizontalOffset(int value)
{
    value = std::clamp(value, -256, 256);
    if (value == horizontalOffset_) return;
    recordUndo();
    horizontalOffset_ = value;
    settingsWereChanged(true);
}

void ImageInputController::setVerticalOffset(int value)
{
    value = std::clamp(value, -192, 192);
    if (value == verticalOffset_) return;
    recordUndo();
    verticalOffset_ = value;
    settingsWereChanged(true);
}

void ImageInputController::setAutoUpdate(bool value)
{
    if (value == autoUpdate_) return;
    autoUpdate_ = value;
    saveSettings();
    emit settingsChanged();
    if (autoUpdate_) {
        if (conversionPending_) scheduleConversion();
        return;
    }
    const bool updateWasInFlight = busy_ || debounceTimer_.isActive();
    jobs_.cancelCurrent();
    debounceTimer_.stop();
    busy_ = false;
    conversionPending_ = updateWasInFlight;
    if (conversionPending_) {
        statusMessage_ = tr("Automatic updates are off. Select Update to convert.");
    } else if (image_) {
        statusMessage_ = tr("Automatic updates are off.");
    }
    emit conversionChanged();
    emit statusChanged();
}

void ImageInputController::setLivePreview(bool value)
{
    if (value == livePreview_) return;
    livePreview_ = value;
    saveSettings();
    emit settingsChanged();
}

void ImageInputController::setBackgroundColor(const QColor& value)
{
    if (!value.isValid()) return;
    const core::RgbColor selected{static_cast<std::uint8_t>(value.red()),
                                  static_cast<std::uint8_t>(value.green()),
                                  static_cast<std::uint8_t>(value.blue())};
    if (selected == backgroundColor_) return;
    recordUndo();
    backgroundColor_ = selected;
    settingsWereChanged(true);
}

void ImageInputController::setForegroundColor(const QColor& value)
{
    if (!value.isValid()) return;
    const core::RgbColor selected{static_cast<std::uint8_t>(value.red()),
                                  static_cast<std::uint8_t>(value.green()),
                                  static_cast<std::uint8_t>(value.blue())};
    if (selected == foregroundColor_) return;
    recordUndo();
    foregroundColor_ = selected;
    saveSettings();
    emit settingsChanged();
}

void ImageInputController::setWorkingPaletteColor(int index, const QColor& color)
{
    if (index < 0 || static_cast<std::size_t>(index) >= workingPalette_.size()
        || !color.isValid()) return;
    const core::RgbColor selected{static_cast<std::uint8_t>(color.red()),
                                  static_cast<std::uint8_t>(color.green()),
                                  static_cast<std::uint8_t>(color.blue())};
    if (workingPalette_[static_cast<std::size_t>(index)] == selected) return;
    recordUndo();
    workingPalette_[static_cast<std::size_t>(index)] = selected;
    backgroundColor_ = nearestBackgroundColor(backgroundColor(), workingPalette_);
    settingsWereChanged(true);
}

void ImageInputController::resetWorkingPalette()
{
    const auto defaults = defaultWorkingColors();
    if (workingPalette_ == defaults) return;
    recordUndo();
    workingPalette_ = defaults;
    backgroundColor_ = nearestBackgroundColor(backgroundColor(), workingPalette_);
    settingsWereChanged(true);
}
