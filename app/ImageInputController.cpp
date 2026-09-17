#include "ImageInputController.hpp"

#include "newconvert9918/core/Bitmap9918Converter.hpp"
#include "newconvert9918/core/F18AConverter.hpp"
#include "newconvert9918/core/ImageAdjustments.hpp"
#include "newconvert9918/core/Multicolor9918Converter.hpp"
#include "newconvert9918/core/PaletteSelection.hpp"
#include "newconvert9918/imageio/ExportWriter.hpp"

#include <QBuffer>
#include <QClipboard>
#include <QColor>
#include <QCoreApplication>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QMetaObject>
#include <QMimeData>
#include <QPointer>
#include <QRegularExpression>
#include <QSettings>
#include <QThreadPool>

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <span>
#include <utility>

namespace {

using namespace newconvert9918;

core::ConversionResult failedResult(std::string code, std::string message)
{
    core::ConversionResult result;
    result.status = core::ConversionStatus::Failed;
    result.diagnostics.push_back(
        {core::DiagnosticSeverity::Error, std::move(code), std::move(message)});
    return result;
}

core::ConversionResult cancelledResult()
{
    core::ConversionResult result;
    result.status = core::ConversionStatus::Cancelled;
    return result;
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
                                     int verticalOffset)
{
    if (!request.source) {
        return failedResult("preview-missing-source", "No source image is loaded.");
    }
    if (request.cancellation.isCancellationRequested()) return cancelledResult();

    const core::ImageTransformOptions transformOptions{
        .targetWidth = 256,
        .targetHeight = 192,
        .filter = scalingFilter,
        .fillMode = fillMode,
        .horizontalOffset = horizontalOffset,
        .verticalOffset = verticalOffset,
    };
    auto transformed = core::transformImage(*request.source, transformOptions);
    if (!transformed) {
        return failedResult("preview-transform-failed",
                            "The source image could not be scaled to 256×192.");
    }
    if (request.cancellation.isCancellationRequested()) return cancelledResult();

    auto adjusted = core::adjustImage(*transformed.image, request.settings);
    if (!adjusted) {
        return failedResult("preview-adjustment-failed",
                            "The selected histogram or gamma adjustment failed.");
    }
    if (request.cancellation.isCancellationRequested()) return cancelledResult();

    const core::Palette colorPalette = core::defaultBitmap9918Palette();
    switch (request.settings.mode) {
    case core::ConversionMode::Bitmap9918:
        return core::convertBitmap9918(*adjusted.image, colorPalette, request.settings);
    case core::ConversionMode::GreyscaleBitmap9918:
        return core::convertGreyscaleBitmap9918(
            *adjusted.image, core::greyscaleBitmap9918Palette(colorPalette), request.settings);
    case core::ConversionMode::BlackAndWhiteBitmap9918:
        return core::convertBlackAndWhiteBitmap9918(
            *adjusted.image, colorPalette, request.settings);
    case core::ConversionMode::Multicolor9918:
        return core::convertMulticolor9918(*adjusted.image, colorPalette, request.settings);
    case core::ConversionMode::DualMulticolor9918:
        return core::convertDualMulticolor9918(
            *adjusted.image, colorPalette, request.settings);
    case core::ConversionMode::HalfMulticolor9918:
        return core::convertHalfMulticolor9918(
            *adjusted.image, colorPalette, request.settings);
    case core::ConversionMode::BitmapColorOnly9918:
        return core::convertBitmapColorOnly9918(
            *adjusted.image, colorPalette, request.settings);
    case core::ConversionMode::PalettedBitmapF18A: {
        auto selected = core::selectMedianCutPalette(
            *adjusted.image, 15, core::MedianCutColorDepth::Rgb444);
        if (!selected) {
            return failedResult("preview-palette-failed",
                                "A 15-color F18A palette could not be selected.");
        }
        return core::convertPalettedBitmapF18A(
            *adjusted.image, *selected.palette, request.settings);
    }
    case core::ConversionMode::ScanlinePaletteBitmapF18A:
        return core::convertScanlinePaletteBitmapF18A(*adjusted.image, request.settings);
    }
    return failedResult("preview-unsupported-mode", "The conversion mode is unsupported.");
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

} // namespace

ImageInputController::ImageInputController(QObject* parent)
    : QObject(parent)
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

void ImageInputController::accept(imageio::ImageLoadResult result, QString sourceName)
{
    if (!result) {
        errorMessage_ = result.error;
        statusMessage_.clear();
        emit statusChanged();
        return;
    }

    sourcePreview_ = dataUrl(*result.image);
    if (sourcePreview_.isEmpty()) {
        errorMessage_ = tr("The decoded image could not be prepared for display.");
        emit statusChanged();
        return;
    }
    image_ = std::make_shared<core::RgbImage>(std::move(*result.image));
    sourceName_ = std::move(sourceName);
    sourceDetails_ = tr("%1 — %2×%3 — %4%5")
        .arg(result.metadata.formatName)
        .arg(image_->width())
        .arg(image_->height())
        .arg(result.metadata.colorSpace)
        .arg(result.metadata.animated
                 ? tr(" — first of %1 frames").arg(result.metadata.frameCount)
                 : QString{});
    errorMessage_.clear();
    statusMessage_ = tr("Image loaded. Preparing preview…");
    convertedPreview_.clear();
    conversionDetails_.clear();
    result_.reset();
    emit sourceChanged();
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
    accept(imageio::loadImageFile(path), QFileInfo(path).fileName());
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
    accept(imageio::loadClipboardImage(clipboard->image()), tr("Clipboard image"));
}

void ImageInputController::scheduleConversion()
{
    jobs_.cancelCurrent();
    if (!image_) return;
    busy_ = true;
    statusMessage_ = tr("Updating preview…");
    emit conversionChanged();
    emit statusChanged();
    debounceTimer_.start();
}

void ImageInputController::startConversion()
{
    if (!image_) return;
    auto request = jobs_.begin(image_, settings_);
    const auto scalingFilter = scalingFilter_;
    const auto fillMode = fillMode_;
    const int horizontalOffset = horizontalOffset_;
    const int verticalOffset = verticalOffset_;
    QPointer<ImageInputController> guarded(this);
    QThreadPool::globalInstance()->start(
        [guarded, request = std::move(request), scalingFilter, fillMode, horizontalOffset,
         verticalOffset]() mutable {
            auto result = runConversion(
                request, scalingFilter, fillMode, horizontalOffset, verticalOffset);
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

ImageInputController::SettingsSnapshot ImageInputController::snapshot() const
{
    return {settings_, scalingFilter_, fillMode_, horizontalOffset_, verticalOffset_};
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
    applyingSnapshot_ = false;
    saveSettings();
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
    applySnapshot(value);
}

void ImageInputController::settingsWereChanged()
{
    saveSettings();
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
        std::clamp(persisted.value(QStringLiteral("dither"), 2).toInt(), 0, 6));
    scalingFilter_ = static_cast<core::ScalingFilter>(
        std::clamp(persisted.value(QStringLiteral("scalingFilter"), 4).toInt(), 0, 5));
    fillMode_ = static_cast<core::ImageFillMode>(
        std::clamp(persisted.value(QStringLiteral("fillMode"), 0).toInt(), 0, 3));
    settings_.perceptualColorMatching =
        persisted.value(QStringLiteral("perceptual"), false).toBool();
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
        std::clamp(persisted.value(QStringLiteral("orderedBrightness"), 0).toInt(), -16, 16);
    horizontalOffset_ =
        std::clamp(persisted.value(QStringLiteral("horizontalOffset"), 0).toInt(), -256, 256);
    verticalOffset_ =
        std::clamp(persisted.value(QStringLiteral("verticalOffset"), 0).toInt(), -192, 192);
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
    persisted.setValue(QStringLiteral("histogram"), stretchHistogram());
    persisted.setValue(QStringLiteral("colorShift"), maximumColorShift());
    persisted.setValue(QStringLiteral("gamma"), gamma());
    persisted.setValue(QStringLiteral("luma"), lumaEmphasis());
    persisted.setValue(QStringLiteral("flicker"), maximumMulticolorDifference());
    persisted.setValue(QStringLiteral("orderedBrightness"), orderedBrightness());
    persisted.setValue(QStringLiteral("horizontalOffset"), horizontalOffset());
    persisted.setValue(QStringLiteral("verticalOffset"), verticalOffset());
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
    value = std::clamp(value, 0, 6);
    if (value == ditherMode()) return;
    recordUndo();
    settings_.dither = static_cast<core::DitherMode>(value);
    settingsWereChanged();
}

void ImageInputController::setScalingFilter(int value)
{
    value = std::clamp(value, 0, 5);
    if (value == scalingFilter()) return;
    recordUndo();
    scalingFilter_ = static_cast<core::ScalingFilter>(value);
    settingsWereChanged();
}

void ImageInputController::setFillMode(int value)
{
    value = std::clamp(value, 0, 3);
    if (value == fillMode()) return;
    recordUndo();
    fillMode_ = static_cast<core::ImageFillMode>(value);
    settingsWereChanged();
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
    value = std::clamp(value, -16, 16);
    if (value == settings_.orderedDitherBrightness) return;
    recordUndo();
    settings_.orderedDitherBrightness = value;
    settingsWereChanged();
}

void ImageInputController::setHorizontalOffset(int value)
{
    value = std::clamp(value, -256, 256);
    if (value == horizontalOffset_) return;
    recordUndo();
    horizontalOffset_ = value;
    settingsWereChanged();
}

void ImageInputController::setVerticalOffset(int value)
{
    value = std::clamp(value, -192, 192);
    if (value == verticalOffset_) return;
    recordUndo();
    verticalOffset_ = value;
    settingsWereChanged();
}
