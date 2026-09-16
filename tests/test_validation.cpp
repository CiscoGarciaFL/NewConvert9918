#include "newconvert9918/core/ColorMath.hpp"
#include "newconvert9918/core/ConversionTypes.hpp"
#include "newconvert9918/core/ImageAdjustments.hpp"
#include "newconvert9918/core/ImageTransform.hpp"
#include "newconvert9918/core/PaletteSelection.hpp"
#include "newconvert9918/core/RgbImage.hpp"
#include "newconvert9918/core/TargetData.hpp"
#include "newconvert9918/core/Validation.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>
#include <QSet>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <string_view>

using newconvert9918::core::ConversionSettings;
using newconvert9918::core::ConversionDiagnostic;
using newconvert9918::core::ConversionMode;
using newconvert9918::core::ConversionRequest;
using newconvert9918::core::ConversionResult;
using newconvert9918::core::ConversionStatus;
using newconvert9918::core::DiagnosticSeverity;
using newconvert9918::core::ImageLayout;
using newconvert9918::core::ImageLayoutError;
using newconvert9918::core::ImageSizeLimits;
using newconvert9918::core::ImageFillMode;
using newconvert9918::core::ImageAdjustmentError;
using newconvert9918::core::ImageTransformError;
using newconvert9918::core::ImageTransformOptions;
using newconvert9918::core::MedianCutColorDepth;
using newconvert9918::core::PerceptualRgbWeights;
using newconvert9918::core::Palette;
using newconvert9918::core::PaletteError;
using newconvert9918::core::PaletteSelectionError;
using newconvert9918::core::PixelFormat;
using newconvert9918::core::PopularityWeighting;
using newconvert9918::core::RgbColor;
using newconvert9918::core::RgbSample;
using newconvert9918::core::RgbImage;
using newconvert9918::core::ScalingFilter;
using newconvert9918::core::TargetMemoryImage;
using newconvert9918::core::TargetMemoryTable;
using newconvert9918::core::TargetTableError;
using newconvert9918::core::TargetTableRole;
using newconvert9918::core::bytesPerPixel;
using newconvert9918::core::adjustImage;
using newconvert9918::core::colorDistanceSquared;
using newconvert9918::core::expectedTargetTables;
using newconvert9918::core::planImageTransform;
using newconvert9918::core::perceptualRgbDistanceSquared;
using newconvert9918::core::toYCrCb;
using newconvert9918::core::selectMedianCutPalette;
using newconvert9918::core::selectPopularPalette;
using newconvert9918::core::transformImage;
using newconvert9918::core::validate;
using newconvert9918::core::validateImageLayout;
using newconvert9918::core::validateTargetTables;
using newconvert9918::core::yCrCbDistanceSquared;

namespace {

const QString corpusDirectory = QStringLiteral(NEWCONVERT9918_GOLDEN_DIR);

class TestContext final {
public:
    void expect(bool condition, std::string_view message)
    {
        if (!condition) {
            ++failures_;
            std::cerr << "FAIL: " << message << '\n';
        }
    }

    void expectNear(double actual, double expected, double tolerance, std::string_view message)
    {
        expect(std::isfinite(actual) && std::abs(actual - expected) <= tolerance, message);
    }

    [[nodiscard]] int result() const { return failures_ == 0 ? 0 : 1; }

private:
    int failures_ = 0;
};

QJsonObject loadCorpusManifest()
{
    QFile file(corpusDirectory + QStringLiteral("/corpus.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject{};
}

QJsonObject loadCaptureManifest()
{
    QFile file(corpusDirectory + QStringLiteral("/reference/original-1_9_1/capture.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject{};
}

QJsonObject loadModeCaptureManifest()
{
    QFile file(corpusDirectory
               + QStringLiteral("/reference/original-1_9_1/mode-captures.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject{};
}

QJsonObject loadExportCaptureManifest()
{
    QFile file(corpusDirectory
               + QStringLiteral("/reference/original-1_9_1/export-captures.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject{};
}

QString corpusPath(const QJsonObject &entry)
{
    const QString repositoryRelativePath = entry.value(QStringLiteral("path")).toString();
    const QString prefix = QStringLiteral("tests/golden/");
    if (!repositoryRelativePath.startsWith(prefix)) {
        return {};
    }
    return corpusDirectory + QStringLiteral("/") + repositoryRelativePath.mid(prefix.size());
}

QByteArray sha256(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        return {};
    }
    return hash.result().toHex();
}

QByteArray readPrefix(const QString &path, qint64 maximumSize)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.read(maximumSize) : QByteArray{};
}

void testSettingsValidation(TestContext &test)
{
    test.expect(validate(ConversionSettings{}).empty(), "default settings should be valid");

    ConversionSettings settings;
    settings.gamma = 0.0;
    auto issues = validate(settings);
    test.expect(issues.size() == 1 && issues.front().field == "gamma",
                "zero gamma should produce one gamma issue");

    settings = ConversionSettings{};
    settings.maximumColorShiftPercent = 101.0;
    issues = validate(settings);
    test.expect(issues.size() == 1 && issues.front().field == "maximumColorShiftPercent",
                "color shift above 100 percent should produce one color-shift issue");

    settings = ConversionSettings{};
    settings.perceptualRedWeight = -0.01;
    issues = validate(settings);
    test.expect(issues.size() == 1 && issues.front().field == "perceptualRedWeight",
                "negative perceptual weights should be rejected");

    settings = ConversionSettings{};
    settings.perceptualRedWeight = 0.0;
    settings.perceptualGreenWeight = 0.0;
    settings.perceptualBlueWeight = 0.0;
    issues = validate(settings);
    test.expect(issues.size() == 1 && issues.front().field == "perceptualColorWeights",
                "at least one perceptual weight should be required");

    settings = ConversionSettings{};
    settings.lumaEmphasis = -0.01;
    issues = validate(settings);
    test.expect(issues.size() == 1 && issues.front().field == "lumaEmphasis",
                "negative luma emphasis should be rejected");

    settings = ConversionSettings{};
    settings.gamma = std::numeric_limits<double>::quiet_NaN();
    issues = validate(settings);
    test.expect(issues.size() == 1 && issues.front().field == "gamma",
                "non-finite settings should be rejected");
}

void testColorMath(TestContext &test)
{
    const RgbSample black{0.0, 0.0, 0.0};
    const RgbSample white{255.0, 255.0, 255.0};
    const RgbSample red{RgbColor{255, 0, 0}};

    const auto whiteYCrCb = toYCrCb(white);
    test.expectNear(whiteYCrCb.luminance, 255.0, 1.0e-12,
                    "white should have full legacy luminance");
    test.expectNear(whiteYCrCb.redChroma, 0.0, 1.0e-12,
                    "white should have zero red chroma");
    test.expectNear(whiteYCrCb.blueChroma, 0.0, 1.0e-12,
                    "white should have zero blue chroma");

    const auto redYCrCb = toYCrCb(red);
    test.expectNear(redYCrCb.luminance, 76.245, 1.0e-12,
                    "red luminance should match the original matrix");
    test.expectNear(redYCrCb.redChroma, 127.5, 1.0e-12,
                    "red chroma should match the original matrix");
    test.expectNear(redYCrCb.blueChroma, -43.095, 1.0e-12,
                    "blue chroma should match the original matrix");

    test.expectNear(yCrCbDistanceSquared(red, black), 26484.581061, 1.0e-9,
                    "default YCrCb distance should preserve legacy luma emphasis");
    test.expectNear(yCrCbDistanceSquared(white, black), 93636.0, 1.0e-9,
                    "neutral YCrCb distance should apply luma emphasis before squaring");
    test.expectNear(perceptualRgbDistanceSquared(red, black), 19507.5, 1.0e-9,
                    "perceptual RGB distance should weight squared channel differences");

    const RgbSample diffused{-12.5, 260.0, 40.25};
    test.expectNear(yCrCbDistanceSquared(diffused, diffused), 0.0, 1.0e-12,
                    "color math should accept identical out-of-range diffusion values");
    test.expectNear(yCrCbDistanceSquared(red, white), yCrCbDistanceSquared(white, red),
                    1.0e-12, "YCrCb distance should be symmetric");

    ConversionSettings settings;
    test.expectNear(colorDistanceSquared(red, black, settings), 26484.581061, 1.0e-9,
                    "conversion settings should select YCrCb matching by default");
    settings.perceptualColorMatching = true;
    test.expectNear(colorDistanceSquared(red, black, settings), 19507.5, 1.0e-9,
                    "conversion settings should select perceptual RGB matching");
    settings.perceptualRedWeight = 1.0;
    settings.perceptualGreenWeight = 0.0;
    settings.perceptualBlueWeight = 0.0;
    test.expectNear(colorDistanceSquared(red, black, settings), 65025.0, 1.0e-9,
                    "custom perceptual weights should flow into color matching");

    test.expectNear(perceptualRgbDistanceSquared(
                        RgbSample{1.0, 2.0, 3.0},
                        RgbSample{4.0, 6.0, 8.0},
                        PerceptualRgbWeights{1.0, 2.0, 3.0}),
                    116.0,
                    1.0e-12,
                    "perceptual weights should scale squared differences, not channels");
}

void testImageAdjustments(TestContext &test)
{
    auto rgba = RgbImage::create(
        {.width = 2, .height = 1, .pixelFormat = PixelFormat::Rgba8888, .rowStride = 10},
        {0, 16, 64, 7, 128, 255, 32, 9, 201, 202});
    test.expect(rgba.has_value(), "image-adjustment fixture should be created");

    auto adjusted = adjustImage(*rgba, ConversionSettings{});
    test.expect(static_cast<bool>(adjusted) && adjusted.image->bytes() == rgba->bytes(),
                "default adjustments should copy every pixel and padding byte unchanged");

    ConversionSettings settings;
    settings.gamma = 2.0;
    adjusted = adjustImage(*rgba, settings);
    test.expect(static_cast<bool>(adjusted), "positive gamma should produce an image");
    test.expect(adjusted.image->bytes()
                    == std::vector<std::uint8_t>(
                        {0, 63, 127, 7, 180, 255, 90, 9, 201, 202}),
                "gamma should use the legacy inverse exponent and preserve alpha and padding");

    settings.gamma = 0.5;
    adjusted = adjustImage(*rgba, settings);
    test.expect(static_cast<bool>(adjusted)
                    && adjusted.image->bytes()[4] == 64
                    && adjusted.image->bytes()[5] == 255
                    && adjusted.image->bytes()[7] == 9,
                "gamma below one should darken RGB without changing alpha");

    auto greys = RgbImage::create(
        {.width = 4, .height = 1, .pixelFormat = PixelFormat::Rgb888, .rowStride = 12},
        {10, 10, 10, 20, 20, 20, 30, 30, 30, 40, 40, 40});
    settings = ConversionSettings{};
    settings.stretchHistogram = true;
    adjusted = adjustImage(*greys, settings);
    test.expect(static_cast<bool>(adjusted)
                    && adjusted.image->bytes()
                        == std::vector<std::uint8_t>(
                            {32, 32, 32, 96, 96, 96, 160, 160, 160, 224, 224, 224}),
                "histogram stretching should equalize brightness into the legacy 32-224 range");

    auto colors = RgbImage::create(
        {.width = 4, .height = 1, .pixelFormat = PixelFormat::Rgb888, .rowStride = 12},
        {10, 20, 30, 60, 70, 80, 110, 120, 130, 160, 170, 180});
    adjusted = adjustImage(*colors, settings);
    test.expect(static_cast<bool>(adjusted)
                    && adjusted.image->bytes()
                        == std::vector<std::uint8_t>(
                            {24, 34, 44, 88, 98, 108, 152, 162, 172, 216, 226, 236}),
                "brightness stretching should preserve RGB channel differences when unclipped");

    settings.gamma = 2.0;
    adjusted = adjustImage(*greys, settings);
    test.expect(static_cast<bool>(adjusted)
                    && adjusted.image->bytes()
                        == std::vector<std::uint8_t>(
                            {90, 90, 90, 156, 156, 156, 201, 201, 201, 238, 238, 238}),
                "histogram stretching should run before gamma correction");

    auto flat = RgbImage::create(
        {.width = 2, .height = 1, .pixelFormat = PixelFormat::Rgb888, .rowStride = 6},
        {40, 50, 60, 40, 50, 60});
    settings.gamma = 1.0;
    adjusted = adjustImage(*flat, settings);
    test.expect(static_cast<bool>(adjusted) && adjusted.image->bytes() == flat->bytes(),
                "a single-valued brightness histogram should remain unchanged");

    settings.gamma = 0.0;
    adjusted = adjustImage(*flat, settings);
    test.expect(!adjusted && adjusted.error == ImageAdjustmentError::InvalidGamma,
                "image adjustment should reject invalid gamma without relying on prior validation");

    settings = ConversionSettings{};
    const ImageSizeLimits smallLimits{
        .maximumWidth = 1,
        .maximumHeight = 1,
        .maximumPixels = 1,
        .maximumBytes = 4,
    };
    adjusted = adjustImage(*flat, settings, smallLimits);
    test.expect(!adjusted && adjusted.error == ImageAdjustmentError::OutputImageRejected,
                "image adjustment should enforce output allocation limits");
}

void testPaletteSelection(TestContext &test)
{
    auto ramp = RgbImage::create(
        {.width = 8, .height = 1, .pixelFormat = PixelFormat::Rgba8888, .rowStride = 34},
        {
            0, 0, 0, 1,
            16, 0, 0, 2,
            32, 0, 0, 3,
            48, 0, 0, 4,
            192, 0, 0, 5,
            208, 0, 0, 6,
            224, 0, 0, 7,
            240, 0, 0, 8,
            201, 202,
        });
    test.expect(ramp.has_value(), "palette-selection fixture should be created");

    auto selected = selectMedianCutPalette(*ramp, 2);
    test.expect(static_cast<bool>(selected) && selected.palette->size() == 2,
                "median cut should produce the requested palette size");
    test.expect(selected.palette->at(0) == RgbColor{17, 0, 0}
                    && selected.palette->at(1) == RgbColor{221, 0, 0},
                "RGB444 median cut should split the longest range and truncate block averages");

    selected = selectMedianCutPalette(*ramp, 2, MedianCutColorDepth::Rgb888);
    test.expect(static_cast<bool>(selected)
                    && selected.palette->at(0) == RgbColor{24, 0, 0}
                    && selected.palette->at(1) == RgbColor{216, 0, 0},
                "RGB888 median cut should retain full channel precision");

    const auto repeated = selectMedianCutPalette(*ramp, 4);
    const auto repeatedAgain = selectMedianCutPalette(*ramp, 4);
    test.expect(static_cast<bool>(repeated) && static_cast<bool>(repeatedAgain)
                    && repeated.palette->colors().size() == 4
                    && std::equal(repeated.palette->colors().begin(),
                                  repeated.palette->colors().end(),
                                  repeatedAgain.palette->colors().begin()),
                "median-cut tie handling should be deterministic");

    auto twoPixels = RgbImage::create(
        {.width = 2, .height = 1, .pixelFormat = PixelFormat::Rgb888, .rowStride = 6},
        {10, 20, 30, 20, 40, 60});
    selected = selectMedianCutPalette(*twoPixels, 1, MedianCutColorDepth::Rgb888);
    test.expect(static_cast<bool>(selected)
                    && selected.palette->at(0) == RgbColor{15, 30, 45},
                "a one-color median palette should be the truncated RGB average");

    auto tiedRanges = RgbImage::create(
        {.width = 4, .height = 1, .pixelFormat = PixelFormat::Rgb888, .rowStride = 12},
        {0, 0, 0, 0, 255, 0, 255, 0, 0, 255, 255, 0});
    selected = selectMedianCutPalette(*tiedRanges, 2, MedianCutColorDepth::Rgb888);
    test.expect(static_cast<bool>(selected)
                    && selected.palette->at(0) == RgbColor{0, 127, 0}
                    && selected.palette->at(1) == RgbColor{255, 127, 0},
                "equal channel ranges should retain the original red-green-blue precedence");

    std::vector<std::uint8_t> popularityPixels;
    const auto append = [&popularityPixels](RgbColor color, std::size_t count) {
        for (std::size_t index = 0; index < count; ++index) {
            popularityPixels.push_back(color.red);
            popularityPixels.push_back(color.green);
            popularityPixels.push_back(color.blue);
        }
    };
    append({0x10, 0x10, 0x10}, 10);
    append({0x20, 0x20, 0x20}, 9);
    append({0xf0, 0x00, 0x00}, 8);
    auto popularitySource = RgbImage::create(
        {.width = 27, .height = 1, .pixelFormat = PixelFormat::Rgb888, .rowStride = 81},
        std::move(popularityPixels));
    selected = selectPopularPalette(*popularitySource, 2, PopularityWeighting::Uniform);
    test.expect(static_cast<bool>(selected) && selected.palette->size() == 2,
                "popularity selection should produce the requested number of colors");
    test.expect(selected.palette->at(0) == RgbColor{17, 17, 17}
                    && selected.palette->at(1) == RgbColor{255, 0, 0},
                "nearby popular RGB444 colors should merge before final ranking");

    auto weightedSource = RgbImage::create(
        {.width = 8, .height = 1, .pixelFormat = PixelFormat::Rgb888, .rowStride = 24},
        {
            0xf0, 0, 0,
            0xf0, 0, 0,
            0, 0xf0, 0,
            0, 0, 0xf0,
            0, 0, 0xf0,
            0xf0, 0xf0, 0,
            0, 0xf0, 0xf0,
            0xf0, 0, 0,
        });
    const auto uniform = selectPopularPalette(
        *weightedSource, 1, PopularityWeighting::Uniform);
    const auto centerWeighted = selectPopularPalette(
        *weightedSource, 1, PopularityWeighting::HorizontalCenter);
    test.expect(static_cast<bool>(uniform)
                    && uniform.palette->at(0) == RgbColor{255, 0, 0}
                    && static_cast<bool>(centerWeighted)
                    && centerWeighted.palette->at(0) == RgbColor{0, 0, 255},
                "horizontal center weighting should reproduce the original 1-2-2-3-3-2-2-1 emphasis");

    auto ties = RgbImage::create(
        {.width = 2, .height = 1, .pixelFormat = PixelFormat::Rgb888, .rowStride = 6},
        {0xf0, 0, 0, 0, 0, 0xf0});
    selected = selectPopularPalette(*ties, 2, PopularityWeighting::Uniform);
    test.expect(static_cast<bool>(selected)
                    && selected.palette->at(0) == RgbColor{0, 0, 255}
                    && selected.palette->at(1) == RgbColor{255, 0, 0},
                "popularity ties should use ascending RGB444 value order");

    selected = selectMedianCutPalette(*ramp, 0);
    test.expect(!selected && selected.error == PaletteSelectionError::InvalidColorCount,
                "palette selection should reject a zero color request");
    selected = selectPopularPalette(
        *ramp, Palette::maximumColorCount + 1, PopularityWeighting::Uniform);
    test.expect(!selected && selected.error == PaletteSelectionError::InvalidColorCount,
                "palette selection should reject more than sixteen colors");
    selected = selectMedianCutPalette(
        *ramp, 2, static_cast<MedianCutColorDepth>(255));
    test.expect(!selected && selected.error == PaletteSelectionError::UnsupportedColorDepth,
                "median cut should reject unknown color-depth options");
    selected = selectPopularPalette(
        *ramp, 2, static_cast<PopularityWeighting>(255));
    test.expect(!selected && selected.error == PaletteSelectionError::UnsupportedWeighting,
                "popularity selection should reject unknown weighting options");
}

void testRgbImage(TestContext &test)
{
    test.expect(bytesPerPixel(PixelFormat::Rgb888) == 3,
                "RGB888 should contain three bytes per pixel");
    test.expect(bytesPerPixel(PixelFormat::Rgba8888) == 4,
                "RGBA8888 should contain four bytes per pixel");
    test.expect(validateImageLayout({0, 1, PixelFormat::Rgb888, 0}).error
                    == ImageLayoutError::ZeroWidth,
                "image width should be positive");
    test.expect(validateImageLayout({1, 0, PixelFormat::Rgb888, 3}).error
                    == ImageLayoutError::ZeroHeight,
                "image height should be positive");
    test.expect(validateImageLayout({1, 1, static_cast<PixelFormat>(255), 4}).error
                    == ImageLayoutError::UnsupportedPixelFormat,
                "unknown pixel formats should be rejected");

    ImageLayoutError error = ImageLayoutError::DataSizeMismatch;
    auto image = RgbImage::createTightlyPacked(2, 3, PixelFormat::Rgb888, {}, &error);
    test.expect(image.has_value() && error == ImageLayoutError::None,
                "a small tightly packed RGB image should be created");
    test.expect(image->rowStride() == 6 && image->bytes().size() == 18,
                "a tightly packed image should derive stride and byte size");
    test.expect(image->row(2).size() == 6,
                "an image row should expose the complete stored stride");

    const ImageLayout paddedLayout{
        .width = 2,
        .height = 2,
        .pixelFormat = PixelFormat::Rgb888,
        .rowStride = 8,
    };
    image = RgbImage::create(paddedLayout, std::vector<std::uint8_t>(16), {}, &error);
    test.expect(image.has_value() && image->minimumRowBytes() == 6
                    && image->rowStride() == 8,
                "RgbImage should preserve explicit row padding");

    const ImageLayout shortStride{
        .width = 2,
        .height = 1,
        .pixelFormat = PixelFormat::Rgba8888,
        .rowStride = 7,
    };
    test.expect(validateImageLayout(shortStride).error == ImageLayoutError::RowStrideTooSmall,
                "row stride must contain every pixel in a row");

    const ImageSizeLimits smallLimits{
        .maximumWidth = 4,
        .maximumHeight = 4,
        .maximumPixels = 8,
        .maximumBytes = 32,
    };
    test.expect(validateImageLayout({5, 1, PixelFormat::Rgb888, 15}, smallLimits).error
                    == ImageLayoutError::DimensionLimitExceeded,
                "image dimensions should respect configured limits");
    test.expect(validateImageLayout({4, 3, PixelFormat::Rgb888, 12}, smallLimits).error
                    == ImageLayoutError::PixelLimitExceeded,
                "image pixel count should respect configured limits");
    test.expect(validateImageLayout({4, 2, PixelFormat::Rgba8888, 20}, smallLimits).error
                    == ImageLayoutError::ByteLimitExceeded,
                "image allocation size should respect configured limits");

    image = RgbImage::create({2, 2, PixelFormat::Rgb888, 6},
                             std::vector<std::uint8_t>(11),
                             {},
                             &error);
    test.expect(!image.has_value() && error == ImageLayoutError::DataSizeMismatch,
                "RgbImage should reject a buffer whose size does not match its layout");

    const ImageSizeLimits overflowLimits{
        .maximumWidth = std::numeric_limits<std::uint32_t>::max(),
        .maximumHeight = std::numeric_limits<std::uint32_t>::max(),
        .maximumPixels = std::numeric_limits<std::size_t>::max(),
        .maximumBytes = std::numeric_limits<std::size_t>::max(),
    };
    const ImageLayout overflowingLayout{
        .width = 1,
        .height = 2,
        .pixelFormat = PixelFormat::Rgb888,
        .rowStride = std::numeric_limits<std::size_t>::max(),
    };
    test.expect(validateImageLayout(overflowingLayout, overflowLimits).error
                    == ImageLayoutError::ByteSizeOverflow,
                "row-stride multiplication should reject integer overflow");
}

void testConversionTypes(TestContext &test)
{
    auto source = RgbImage::createTightlyPacked(2, 2, PixelFormat::Rgba8888);
    test.expect(source.has_value(), "conversion-type fixture should be created");

    ConversionRequest request{
        .source = std::make_shared<const RgbImage>(std::move(*source)),
        .settings = {},
    };
    test.expect(validate(request).empty(), "a request with a source and valid settings should pass");

    request.settings.gamma = 0.0;
    const auto invalidSettings = validate(request);
    test.expect(invalidSettings.size() == 1 && invalidSettings.front().field == "gamma",
                "request validation should include settings issues");

    request.source.reset();
    const auto invalidRequest = validate(request);
    test.expect(invalidRequest.size() == 2 && invalidRequest.front().field == "source",
                "request validation should report a missing source before settings issues");

    ConversionResult result{
        .status = ConversionStatus::Succeeded,
        .preview = std::nullopt,
        .diagnostics = {{DiagnosticSeverity::Warning, "palette-reduced", "Palette reduced."}},
    };
    test.expect(result.succeeded() && !result.hasErrors(),
                "warnings should not turn a successful conversion into a failure");

    result.diagnostics.push_back(
        ConversionDiagnostic{DiagnosticSeverity::Error, "invalid-source", "Invalid source."});
    test.expect(result.hasErrors() && !result.succeeded(),
                "error diagnostics should make a result unsuccessful");

    result.status = ConversionStatus::Cancelled;
    result.diagnostics.clear();
    test.expect(!result.succeeded() && !result.hasErrors(),
                "cancellation should remain distinct from an error diagnostic");
}

void testTargetData(TestContext &test)
{
    PaletteError paletteError = PaletteError::TooManyColors;
    auto palette = Palette::create({RgbColor{0, 0, 0}, RgbColor{255, 255, 255}},
                                   &paletteError);
    test.expect(palette.has_value() && paletteError == PaletteError::None,
                "a palette containing one to sixteen colors should be created");
    test.expect(palette->size() == 2 && palette->at(1) == RgbColor{255, 255, 255},
                "palette entries should preserve their RGB channel values");

    palette = Palette::create({}, &paletteError);
    test.expect(!palette.has_value() && paletteError == PaletteError::Empty,
                "an empty palette should be rejected");
    palette = Palette::create(std::vector<RgbColor>(Palette::maximumColorCount + 1),
                              &paletteError);
    test.expect(!palette.has_value() && paletteError == PaletteError::TooManyColors,
                "a palette with more than sixteen colors should be rejected");

    const std::array modes{
        ConversionMode::Bitmap9918,
        ConversionMode::GreyscaleBitmap9918,
        ConversionMode::BlackAndWhiteBitmap9918,
        ConversionMode::Multicolor9918,
        ConversionMode::DualMulticolor9918,
        ConversionMode::HalfMulticolor9918,
        ConversionMode::BitmapColorOnly9918,
        ConversionMode::PalettedBitmapF18A,
        ConversionMode::ScanlinePaletteBitmapF18A,
    };

    for (const auto mode : modes) {
        const auto expected = expectedTargetTables(mode);
        test.expect(!expected.empty(), "every conversion mode should define target tables");

        std::vector<TargetMemoryTable> tables;
        for (const auto layout : expected) {
            tables.push_back({layout.role, std::vector<std::uint8_t>(layout.byteSize)});
        }
        test.expect(static_cast<bool>(validateTargetTables(mode, tables)),
                    "tables matching their mode layout should be valid");
    }

    const auto bitmapLayout = expectedTargetTables(ConversionMode::Bitmap9918);
    test.expect(bitmapLayout.size() == 2
                    && bitmapLayout[0].role == TargetTableRole::Pattern
                    && bitmapLayout[0].byteSize == 6144
                    && bitmapLayout[1].role == TargetTableRole::Color
                    && bitmapLayout[1].byteSize == 6144,
                "Bitmap 9918A should define 6144-byte pattern and color tables");
    const auto blackAndWhiteLayout =
        expectedTargetTables(ConversionMode::BlackAndWhiteBitmap9918);
    test.expect(blackAndWhiteLayout.size() == 1
                    && blackAndWhiteLayout[0].role == TargetTableRole::Pattern
                    && blackAndWhiteLayout[0].byteSize == 6144,
                "black-and-white mode should define only a 6144-byte pattern table");
    const auto multicolorLayout = expectedTargetTables(ConversionMode::Multicolor9918);
    test.expect(multicolorLayout.size() == 1
                    && multicolorLayout[0].role == TargetTableRole::Multicolor
                    && multicolorLayout[0].byteSize == 1536,
                "Multicolor 9918 should define one 1536-byte table");
    const auto dualLayout = expectedTargetTables(ConversionMode::DualMulticolor9918);
    test.expect(dualLayout.size() == 2
                    && dualLayout[0].role == TargetTableRole::MulticolorFrame1
                    && dualLayout[1].role == TargetTableRole::MulticolorFrame2
                    && dualLayout[0].byteSize == 1536 && dualLayout[1].byteSize == 1536,
                "dual multicolor should define two 1536-byte frame tables");
    const auto halfLayout = expectedTargetTables(ConversionMode::HalfMulticolor9918);
    test.expect(halfLayout.size() == 3 && halfLayout[2].role == TargetTableRole::Multicolor
                    && halfLayout[2].byteSize == 2048,
                "half multicolor should include its 2048-byte multicolor table");
    const auto f18aLayout = expectedTargetTables(ConversionMode::PalettedBitmapF18A);
    test.expect(f18aLayout.size() == 3 && f18aLayout[2].role == TargetTableRole::Palette
                    && f18aLayout[2].byteSize == 32,
                "paletted F18A mode should include a 32-byte palette table");
    const auto scanlineLayout = expectedTargetTables(ConversionMode::ScanlinePaletteBitmapF18A);
    test.expect(scanlineLayout.size() == 3
                    && scanlineLayout[2].role == TargetTableRole::ScanlinePalettes
                    && scanlineLayout[2].byteSize == 6144,
                "scanline F18A mode should include a 6144-byte palette table");

    test.expect(validateTargetTables(static_cast<ConversionMode>(255), {}).error
                    == TargetTableError::UnsupportedConversionMode,
                "unknown conversion modes should not validate as empty target layouts");

    std::vector<TargetMemoryTable> bitmapTables{
        {bitmapLayout[0].role, std::vector<std::uint8_t>(bitmapLayout[0].byteSize)},
        {bitmapLayout[1].role, std::vector<std::uint8_t>(bitmapLayout[1].byteSize)},
    };
    bitmapTables.pop_back();
    auto invalidTables = validateTargetTables(ConversionMode::Bitmap9918, bitmapTables);
    test.expect(invalidTables.error == TargetTableError::TableCountMismatch,
                "a mode should reject a missing target table");

    bitmapTables.push_back(
        {TargetTableRole::Palette, std::vector<std::uint8_t>(bitmapLayout[1].byteSize)});
    invalidTables = validateTargetTables(ConversionMode::Bitmap9918, bitmapTables);
    test.expect(invalidTables.error == TargetTableError::TableRoleMismatch,
                "a mode should reject a target table in the wrong role");

    bitmapTables[1].role = bitmapLayout[1].role;
    bitmapTables[1].bytes.pop_back();
    const auto invalidSize = validateTargetTables(ConversionMode::Bitmap9918, bitmapTables);
    test.expect(invalidSize.error == TargetTableError::TableSizeMismatch
                    && invalidSize.tableIndex == 1
                    && invalidSize.expected.byteSize == bitmapLayout[1].byteSize,
                "target-table size errors should identify the table and expected layout");

    const TargetMemoryImage target{
        .mode = ConversionMode::Bitmap9918,
        .palette = std::nullopt,
        .tables = {
            {bitmapLayout[0].role, std::vector<std::uint8_t>(bitmapLayout[0].byteSize)},
            {bitmapLayout[1].role, std::vector<std::uint8_t>(bitmapLayout[1].byteSize)},
        },
    };
    test.expect(static_cast<bool>(validateTargetTables(target)),
                "a target-memory image should validate against its recorded mode");
}

void testImageTransform(TestContext &test)
{
    auto source = RgbImage::create(
        {.width = 4, .height = 1, .pixelFormat = PixelFormat::Rgb888, .rowStride = 12},
        {
            10, 0, 0,
            20, 0, 0,
            30, 0, 0,
            40, 0, 0,
        });
    test.expect(source.has_value(), "image-transform fixture should be created");

    ImageTransformOptions options{
        .targetWidth = 8,
        .targetHeight = 6,
        .filter = ScalingFilter::Bilinear,
        .fillMode = ImageFillMode::Fit,
    };
    auto plan = planImageTransform(*source, options);
    test.expect(plan.has_value() && plan->scaledWidth == 8 && plan->scaledHeight == 2
                    && plan->destinationX == 0 && plan->destinationY == 2,
                "fit mode should preserve aspect ratio and center letterboxing");

    options.targetWidth = 2;
    options.targetHeight = 2;
    options.fillMode = ImageFillMode::CropCenter;
    plan = planImageTransform(*source, options);
    test.expect(plan.has_value() && plan->scaledWidth == 8 && plan->scaledHeight == 2
                    && plan->cropX == 3 && plan->cropY == 0,
                "center fill should scale to cover and crop the middle");

    options.filter = ScalingFilter::None;
    options.targetWidth = 2;
    options.targetHeight = 1;
    options.fillMode = ImageFillMode::CropStart;
    plan = planImageTransform(*source, options);
    test.expect(plan.has_value() && plan->scaledWidth == 4 && plan->cropX == 0,
                "no-scale start crop should retain the first source pixels");
    options.fillMode = ImageFillMode::CropCenter;
    plan = planImageTransform(*source, options);
    test.expect(plan.has_value() && plan->cropX == 1,
                "no-scale center crop should retain the middle source pixels");
    options.fillMode = ImageFillMode::CropEnd;
    plan = planImageTransform(*source, options);
    test.expect(plan.has_value() && plan->cropX == 2,
                "no-scale end crop should retain the final source pixels");

    options.fillMode = ImageFillMode::CropCenter;
    options.horizontalOffset = 10;
    plan = planImageTransform(*source, options);
    test.expect(plan.has_value() && plan->cropX == 2,
                "positive crop offsets should clamp at the final valid pixel");
    options.horizontalOffset = -10;
    plan = planImageTransform(*source, options);
    test.expect(plan.has_value() && plan->cropX == 0,
                "negative crop offsets should clamp at the first valid pixel");

    options.horizontalOffset = 0;
    auto transformed = transformImage(*source, options);
    test.expect(static_cast<bool>(transformed),
                "a valid no-scale crop should produce an image");
    test.expect(transformed.image->bytes()
                    == std::vector<std::uint8_t>({20, 0, 0, 30, 0, 0}),
                "center cropping should copy the expected source pixels");

    options.targetWidth = 6;
    options.targetHeight = 3;
    options.fillMode = ImageFillMode::Fit;
    options.backgroundRed = 1;
    options.backgroundGreen = 2;
    options.backgroundBlue = 3;
    transformed = transformImage(*source, options);
    test.expect(static_cast<bool>(transformed) && transformed.plan.destinationX == 1
                    && transformed.plan.destinationY == 1,
                "an unscaled source should be centered in a larger target");
    test.expect(transformed.image->bytes()[0] == 1 && transformed.image->bytes()[1] == 2
                    && transformed.image->bytes()[2] == 3,
                "letterbox pixels should use the configured background color");

    auto verticalSource = RgbImage::create(
        {.width = 1, .height = 4, .pixelFormat = PixelFormat::Rgb888, .rowStride = 3},
        {10, 0, 0, 20, 0, 0, 30, 0, 0, 40, 0, 0});
    options = {
        .targetWidth = 1,
        .targetHeight = 2,
        .filter = ScalingFilter::None,
        .fillMode = ImageFillMode::CropEnd,
    };
    transformed = transformImage(*verticalSource, options);
    test.expect(static_cast<bool>(transformed) && transformed.plan.cropY == 2
                    && transformed.image->bytes()[0] == 30
                    && transformed.image->bytes()[3] == 40,
                "end cropping should retain the final source rows");

    auto rgbaSource = RgbImage::create(
        {.width = 1, .height = 1, .pixelFormat = PixelFormat::Rgba8888, .rowStride = 4},
        {9, 8, 7, 6});
    options = {
        .targetWidth = 3,
        .targetHeight = 3,
        .filter = ScalingFilter::None,
        .fillMode = ImageFillMode::Fit,
        .backgroundRed = 1,
        .backgroundGreen = 2,
        .backgroundBlue = 3,
        .backgroundAlpha = 4,
    };
    transformed = transformImage(*rgbaSource, options);
    test.expect(static_cast<bool>(transformed) && transformed.image->pixelFormat()
                    == PixelFormat::Rgba8888,
                "image transforms should preserve the source pixel format");
    test.expect(transformed.image->bytes()[0] == 1 && transformed.image->bytes()[3] == 4
                    && transformed.image->bytes()[16] == 9
                    && transformed.image->bytes()[19] == 6,
                "RGBA composition should preserve source and background alpha");

    auto gradient = RgbImage::create(
        {.width = 2, .height = 1, .pixelFormat = PixelFormat::Rgb888, .rowStride = 6},
        {255, 0, 0, 0, 0, 255});
    options = {
        .targetWidth = 3,
        .targetHeight = 1,
        .filter = ScalingFilter::Bilinear,
        .fillMode = ImageFillMode::CropCenter,
    };
    transformed = transformImage(*gradient, options);
    test.expect(static_cast<bool>(transformed) && transformed.image->bytes()[3] == 85
                    && transformed.image->bytes()[5] == 170,
                "bilinear scaling should deterministically blend neighboring pixels");

    for (const ScalingFilter filter : {ScalingFilter::Box,
                                       ScalingFilter::Gaussian,
                                       ScalingFilter::Hamming,
                                       ScalingFilter::Blackman,
                                       ScalingFilter::Bilinear}) {
        options.filter = filter;
        test.expect(static_cast<bool>(transformImage(*gradient, options)),
                    "every supported resampling filter should produce an image");
    }

    options.targetWidth = 0;
    test.expect(transformImage(*gradient, options).error
                    == ImageTransformError::ZeroTargetDimension,
                "zero target dimensions should be rejected");
    options.targetWidth = 3;
    options.filter = static_cast<ScalingFilter>(255);
    test.expect(transformImage(*gradient, options).error
                    == ImageTransformError::UnsupportedFilter,
                "unknown scaling filters should be rejected");
    options.filter = ScalingFilter::Bilinear;
    options.fillMode = static_cast<ImageFillMode>(255);
    test.expect(transformImage(*gradient, options).error
                    == ImageTransformError::UnsupportedFillMode,
                "unknown fill modes should be rejected");

    options = {
        .targetWidth = 1,
        .targetHeight = 4,
        .filter = ScalingFilter::Bilinear,
        .fillMode = ImageFillMode::CropCenter,
    };
    const ImageSizeLimits transformLimits{
        .maximumWidth = 8,
        .maximumHeight = 8,
        .maximumPixels = 64,
        .maximumBytes = 256,
    };
    test.expect(transformImage(*source, options, transformLimits).error
                    == ImageTransformError::ScaledImageLimitExceeded,
                "an oversized resampling intermediate should be rejected before allocation");
}

void testCorpusManifest(TestContext &test, const QJsonObject &manifest)
{
    test.expect(manifest.value(QStringLiteral("schema_version")).toInt() == 1,
                "corpus schema version should be 1");

    const QJsonArray sourceImages = manifest.value(QStringLiteral("source_images")).toArray();
    const QJsonArray malformedInputs = manifest.value(QStringLiteral("malformed_inputs")).toArray();
    test.expect(sourceImages.size() == 8, "corpus should contain eight source images");
    test.expect(malformedInputs.size() == 3, "corpus should contain three malformed inputs");

    for (const QJsonArray &entries : {sourceImages, malformedInputs}) {
        for (const QJsonValue &value : entries) {
            const QJsonObject entry = value.toObject();
            const QString path = corpusPath(entry);
            test.expect(!path.isEmpty(), "corpus paths must remain under tests/golden");
            test.expect(sha256(path) == entry.value(QStringLiteral("sha256")).toString().toLatin1(),
                        "corpus SHA-256 digest should match the manifest");
        }
    }
}

void testCorpusImages(TestContext &test, const QJsonObject &manifest)
{
    const QJsonArray sourceImages = manifest.value(QStringLiteral("source_images")).toArray();
    for (const QJsonValue &value : sourceImages) {
        const QJsonObject entry = value.toObject();
        QImageReader reader(corpusPath(entry));
        const QImage image = reader.read();
        test.expect(!image.isNull(), "each corpus source should decode as an image");
        test.expect(image.size()
                        == QSize(entry.value(QStringLiteral("width")).toInt(),
                                 entry.value(QStringLiteral("height")).toInt()),
                    "decoded dimensions should match the manifest");
    }

    for (const QString &filename : {QStringLiteral("tiny-rgba.png"),
                                    QStringLiteral("transparency-rgba.png")}) {
        const QImage image(corpusDirectory + QStringLiteral("/source/") + filename);
        test.expect(!image.isNull() && image.hasAlphaChannel(),
                    "alpha fixtures should decode with an alpha channel");
    }

    const QImage gradient(corpusDirectory + QStringLiteral("/source/transparency-rgba.png"));
    test.expect(gradient.pixelColor(0, 0).alpha() == 0,
                "the transparency gradient should include fully transparent pixels");
    test.expect(gradient.pixelColor(gradient.width() / 2, gradient.height() / 2).alpha() >= 250,
                "the transparency gradient should include nearly opaque pixels");
}

void testMalformedInputs(TestContext &test, const QJsonObject &manifest)
{
    const QJsonArray malformedInputs = manifest.value(QStringLiteral("malformed_inputs")).toArray();
    for (const QJsonValue &value : malformedInputs) {
        QImageReader reader(corpusPath(value.toObject()));
        test.expect(reader.read().isNull(), "malformed corpus input should be rejected");
    }
}

void testOriginalCapture(TestContext &test)
{
    const QJsonObject capture = loadCaptureManifest();
    test.expect(capture.value(QStringLiteral("schema_version")).toInt() == 1,
                "original capture schema version should be 1");
    const QJsonObject original = capture.value(QStringLiteral("original")).toObject();
    test.expect(original.value(QStringLiteral("version")).toString() == QStringLiteral("1.9.1.0"),
                "original capture should identify Convert9918 1.9.1.0");

    const QJsonArray captures = capture.value(QStringLiteral("captures")).toArray();
    test.expect(captures.size() == 8, "original capture should contain all eight valid sources");
    for (const QJsonValue &captureValue : captures) {
        const QJsonObject captureEntry = captureValue.toObject();
        const QJsonArray outputs = captureEntry.value(QStringLiteral("outputs")).toArray();
        test.expect(outputs.size() == 3, "each original capture should contain three outputs");
        for (const QJsonValue &outputValue : outputs) {
            const QJsonObject output = outputValue.toObject();
            const QString path = corpusPath(output);
            const QFileInfo file(path);
            test.expect(file.exists(), "each recorded original output should exist");
            test.expect(file.size() == output.value(QStringLiteral("size")).toInteger(),
                        "original output size should match its capture manifest");
            test.expect(sha256(path) == output.value(QStringLiteral("sha256")).toString().toLatin1(),
                        "original output SHA-256 should match its capture manifest");

            const QString suffix = file.suffix().toUpper();
            if (suffix == QStringLiteral("BMP")) {
                const QImage preview(path);
                test.expect(preview.size() == QSize(256, 192),
                            "original BMP preview should be 256 by 192 pixels");
            } else {
                test.expect(suffix == QStringLiteral("TIAP") || suffix == QStringLiteral("TIAC"),
                            "original binary output should be a TIAP or TIAC table");
                test.expect(file.size() == 6272,
                            "TIFILES table should contain a 128-byte header and 6144-byte payload");
                QFile input(path);
                test.expect(input.open(QIODevice::ReadOnly), "TIFILES output should be readable");
                test.expect(input.read(8) == QByteArray("\x07TIFILES", 8),
                            "TIFILES output should contain the expected signature");
            }
        }
    }
}

void testOriginalModeCaptures(TestContext &test)
{
    const QJsonObject manifest = loadModeCaptureManifest();
    test.expect(manifest.value(QStringLiteral("schema_version")).toInt() == 1,
                "original mode-capture schema version should be 1");

    const QJsonArray modes = manifest.value(QStringLiteral("modes")).toArray();
    test.expect(modes.size() == 8, "mode captures should contain the eight remaining modes");
    QSet<int> capturedModeIndexes;

    for (const QJsonValue &modeValue : modes) {
        const QJsonObject mode = modeValue.toObject();
        const int modeIndex = mode.value(QStringLiteral("conversion_mode_index")).toInt(-1);
        test.expect(modeIndex >= 1 && modeIndex <= 8,
                    "captured conversion-mode index should be between 1 and 8");
        test.expect(!capturedModeIndexes.contains(modeIndex),
                    "captured conversion-mode indexes should be unique");
        capturedModeIndexes.insert(modeIndex);

        QHash<QString, qint64> expectedPayloadSizes;
        const QJsonArray tablePayloads = mode.value(QStringLiteral("table_payloads")).toArray();
        for (const QJsonValue &tableValue : tablePayloads) {
            const QJsonObject table = tableValue.toObject();
            expectedPayloadSizes.insert(table.value(QStringLiteral("extension")).toString(),
                                        table.value(QStringLiteral("payload_size")).toInteger());
        }

        const bool previewAvailable = mode.value(QStringLiteral("preview_available")).toBool();
        const int expectedOutputCount = tablePayloads.size() + (previewAvailable ? 1 : 0);
        const QJsonArray captures = mode.value(QStringLiteral("captures")).toArray();
        test.expect(captures.size() == 8,
                    "each remaining conversion mode should contain all eight valid sources");

        for (const QJsonValue &captureValue : captures) {
            const QJsonArray outputs =
                captureValue.toObject().value(QStringLiteral("outputs")).toArray();
            test.expect(outputs.size() == expectedOutputCount,
                        "each mode capture should contain its applicable outputs");

            for (const QJsonValue &outputValue : outputs) {
                const QJsonObject output = outputValue.toObject();
                const QString path = corpusPath(output);
                const QFileInfo file(path);
                test.expect(file.exists(), "each recorded mode output should exist");
                test.expect(file.size() == output.value(QStringLiteral("size")).toInteger(),
                            "mode-output size should match its capture manifest");
                test.expect(sha256(path)
                                == output.value(QStringLiteral("sha256")).toString().toLatin1(),
                            "mode-output SHA-256 should match its capture manifest");

                const QString extension = output.value(QStringLiteral("extension")).toString();
                if (output.value(QStringLiteral("kind")).toString()
                    == QStringLiteral("preview")) {
                    const QImage preview(path);
                    test.expect(extension == QStringLiteral("BMP")
                                    && preview.size() == QSize(256, 192),
                                "captured preview should be a 256 by 192 BMP");
                } else {
                    test.expect(expectedPayloadSizes.contains(extension),
                                "captured table extension should be expected for its mode");
                    test.expect(file.size() == expectedPayloadSizes.value(extension) + 128,
                                "captured table should contain its payload and TIFILES header");
                    QFile input(path);
                    test.expect(input.open(QIODevice::ReadOnly),
                                "captured TIFILES table should be readable");
                    test.expect(input.read(8) == QByteArray("\x07TIFILES", 8),
                                "captured table should contain the TIFILES signature");
                }
            }
        }
    }

    for (int modeIndex = 1; modeIndex <= 8; ++modeIndex) {
        test.expect(capturedModeIndexes.contains(modeIndex),
                    "every remaining original conversion-mode index should be captured");
    }
}

void testOriginalExportCaptures(TestContext &test)
{
    const QJsonObject manifest = loadExportCaptureManifest();
    test.expect(manifest.value(QStringLiteral("schema_version")).toInt() == 1,
                "original export-capture schema version should be 1");
    test.expect(manifest.value(QStringLiteral("original"))
                        .toObject()
                        .value(QStringLiteral("version"))
                        .toString()
                    == QStringLiteral("1.9.1.0"),
                "original export capture should identify Convert9918 1.9.1.0");

    const QJsonObject workflow = manifest.value(QStringLiteral("workflow")).toObject();
    test.expect(workflow.value(QStringLiteral("method")).toString()
                    == QStringLiteral("controlled-ui"),
                "export baseline should record the controlled UI workflow");
    test.expect(workflow.value(QStringLiteral("conversion_mode_index")).toInt(-1) == 0,
                "export baseline should use default Bitmap 9918A mode");
    const QJsonObject source = workflow.value(QStringLiteral("source")).toObject();
    test.expect(sha256(corpusPath(source))
                    == source.value(QStringLiteral("sha256")).toString().toLatin1(),
                "export baseline source digest should match its manifest");

    const QSet<QString> expectedFormatIds = {
        QStringLiteral("v9t9"),       QStringLiteral("raw"),
        QStringLiteral("rle"),        QStringLiteral("ti-xb"),
        QStringLiteral("ti-xb-rle"),  QStringLiteral("msx-sc2"),
        QStringLiteral("cvpaint"),    QStringLiteral("powerpaint"),
        QStringLiteral("hgr"),        QStringLiteral("coleco-rom"),
        QStringLiteral("png"),
    };
    QSet<QString> capturedFormatIds;
    int outputCount = 0;

    const QJsonArray formats = manifest.value(QStringLiteral("formats")).toArray();
    test.expect(formats.size() == expectedFormatIds.size(),
                "export baseline should contain all eleven applicable choices");
    for (const QJsonValue &formatValue : formats) {
        const QJsonObject format = formatValue.toObject();
        const QString id = format.value(QStringLiteral("id")).toString();
        test.expect(expectedFormatIds.contains(id),
                    "export capture should use a recognized format id");
        test.expect(!capturedFormatIds.contains(id),
                    "export capture format ids should be unique");
        capturedFormatIds.insert(id);

        const QJsonArray outputs = format.value(QStringLiteral("outputs")).toArray();
        test.expect(!outputs.isEmpty(), "each captured export format should emit a file");
        outputCount += outputs.size();
        for (const QJsonValue &outputValue : outputs) {
            const QJsonObject output = outputValue.toObject();
            const QString path = corpusPath(output);
            const QFileInfo file(path);
            test.expect(file.exists(), "each recorded export output should exist");
            test.expect(file.size() == output.value(QStringLiteral("size")).toInteger(),
                        "export size should match its capture manifest");
            test.expect(sha256(path)
                            == output.value(QStringLiteral("sha256")).toString().toLatin1(),
                        "export SHA-256 should match its capture manifest");

            const QByteArray prefix = readPrefix(path, 16);
            if (id == QStringLiteral("v9t9")) {
                test.expect(file.size() == 6272 && prefix.startsWith("tinyv9_"),
                            "V9T9 tables should have a 128-byte filename header");
                test.expect(!prefix.startsWith(QByteArray("\x07TIFILES", 8)),
                            "V9T9 tables should not contain a TIFILES signature");
            } else if (id == QStringLiteral("raw")) {
                test.expect(file.size() == 6144,
                            "raw pattern and color tables should be exactly 6144 bytes");
            } else if (id == QStringLiteral("rle")) {
                test.expect(file.size() > 0 && file.size() < 6144,
                            "RLE tables should be nonempty and smaller than raw tables");
            } else if (id == QStringLiteral("ti-xb")
                       || id == QStringLiteral("ti-xb-rle")) {
                test.expect(prefix.startsWith(QByteArray("\x07TIFILES", 8)),
                            "TI XB programs should use the original TIFILES wrapper");
            } else if (id == QStringLiteral("msx-sc2")) {
                test.expect(prefix.startsWith(QByteArray("\xFE\x00\x00\x00\x38\x00\x00", 7)),
                            "MSX SC2 should contain the captured binary header");
            } else if (id == QStringLiteral("cvpaint")) {
                test.expect(file.size() == 12288,
                            "CVPaint output should contain two 6144-byte tables");
            } else if (id == QStringLiteral("powerpaint")) {
                test.expect(file.size() == 10240,
                            "PowerPaint output should retain its 10 KiB layout");
            } else if (id == QStringLiteral("hgr")) {
                test.expect(file.size() == 10261
                                && prefix.startsWith(QByteArray("\x01\x00\x02", 3)),
                            "Adam HGR should retain its captured header and size");
            } else if (id == QStringLiteral("coleco-rom")) {
                test.expect(prefix.startsWith(QByteArray("\x55\xAA", 2)),
                            "ColecoVision cartridge should start with its ROM signature");
            } else if (id == QStringLiteral("png")) {
                const QImage image(path);
                test.expect(image.size() == QSize(256, 192),
                            "captured PNG export should be 256 by 192 pixels");
            }
        }
    }

    test.expect(capturedFormatIds == expectedFormatIds,
                "every applicable non-TIFILES export choice should be captured");
    test.expect(outputCount == 14, "the eleven export choices should emit fourteen files");

    const QJsonArray excluded = manifest.value(QStringLiteral("excluded_formats")).toArray();
    test.expect(excluded.size() == 1
                    && excluded.first()
                           .toObject()
                           .value(QStringLiteral("id"))
                           .toString()
                        == QStringLiteral("coleco-rle-rom")
                    && excluded.first()
                           .toObject()
                           .value(QStringLiteral("menu_label"))
                           .toString()
                           .contains(QStringLiteral("Broken")),
                "the original's broken RLE cartridge writer should be explicitly excluded");
}

} // namespace

int main(int argc, char *argv[])
{
    const QCoreApplication application(argc, argv);
    TestContext test;
    testSettingsValidation(test);
    testColorMath(test);
    testImageAdjustments(test);
    testPaletteSelection(test);
    testRgbImage(test);
    testConversionTypes(test);
    testTargetData(test);
    testImageTransform(test);
    const QJsonObject manifest = loadCorpusManifest();
    testCorpusManifest(test, manifest);
    testCorpusImages(test, manifest);
    testMalformedInputs(test, manifest);
    testOriginalCapture(test);
    testOriginalModeCaptures(test);
    testOriginalExportCaptures(test);
    return test.result();
}
