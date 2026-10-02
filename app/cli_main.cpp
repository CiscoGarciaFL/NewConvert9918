#include "ConversionPipeline.hpp"

#include "retrovdp/core/Dithering.hpp"
#include "retrovdp/core/TargetProfile.hpp"
#include "retrovdp/formats/Export.hpp"
#include "retrovdp/imageio/ExportWriter.hpp"
#include "retrovdp/imageio/ImageLoader.hpp"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QString>
#include <QTextStream>

#include <array>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace {

using namespace retrovdp;

enum class ExitCode : int {
    Success = 0,
    Usage = 2,
    Input = 3,
    Conversion = 4,
    Export = 5,
    Write = 6,
};

struct FormatChoice {
    const char* name;
    formats::ExportFormat value;
};

constexpr std::array formatChoices{
    FormatChoice{"tifiles", formats::ExportFormat::TiFiles},
    FormatChoice{"v9t9", formats::ExportFormat::V9t9},
    FormatChoice{"raw", formats::ExportFormat::Raw},
    FormatChoice{"rle", formats::ExportFormat::Rle},
    FormatChoice{"msx-sc2", formats::ExportFormat::MsxScreen2},
    FormatChoice{"coleco-cvpaint", formats::ExportFormat::ColecoCvPaint},
    FormatChoice{"adam-powerpaint", formats::ExportFormat::AdamPowerPaint},
    FormatChoice{"adam-hgr", formats::ExportFormat::AdamHgr},
    FormatChoice{"png", formats::ExportFormat::Png},
};

template <typename Choice, std::size_t Size, typename Value>
std::optional<Value> choiceValue(const std::array<Choice, Size>& choices,
                                 const QString& requested)
{
    for (const auto& choice : choices) {
        if (requested == QLatin1String(choice.name)) return choice.value;
    }
    return std::nullopt;
}

template <typename Choice, std::size_t Size, typename Value>
QString choiceName(const std::array<Choice, Size>& choices, Value value)
{
    for (const auto& choice : choices) {
        if (choice.value == value) return QLatin1String(choice.name);
    }
    return {};
}

bool loadRecipe(const QString& path,
                core::ConversionSettings& settings,
                appsupport::ConversionPipelineOptions& pipeline,
                int& exportFormat,
                QString& sourcePath,
                QString& error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("Could not open recipe: %1").arg(file.errorString());
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("Invalid recipe JSON: %1").arg(parseError.errorString());
        return false;
    }
    const QJsonObject root = document.object();
    const QString recipeKind = root.value(QStringLiteral("kind")).toString();
    if ((recipeKind != QStringLiteral("retrovdp-studio-recipe")
         && recipeKind != QStringLiteral("newconvert9918-recipe"))
        || root.value(QStringLiteral("schemaVersion")).toInt() != 1) {
        error = QStringLiteral("Unsupported recipe type or schema version.");
        return false;
    }
    if (root.value(QStringLiteral("workspace")).toString()
        != QStringLiteral("screen-image")) {
        error = QStringLiteral(
            "Character and sprite recipes are not executable until their pattern converters are implemented.");
        return false;
    }
    const QJsonObject conversion = root.value(QStringLiteral("conversion")).toObject();
    if (conversion.isEmpty()) {
        error = QStringLiteral("Recipe does not contain conversion settings.");
        return false;
    }

    settings.mode = static_cast<core::ConversionMode>(std::clamp(
        conversion.value(QStringLiteral("mode")).toInt(0), 0,
        static_cast<int>(core::ConversionMode::Mode5GenesisH40Pal)));
    const auto savedTarget = core::targetProfileId(
        conversion.value(QStringLiteral("targetProfile")).toString().toStdString());
    settings.targetProfile = core::effectiveTargetProfile(
        savedTarget.value_or(core::primaryTargetProfile(settings.mode)), settings.mode);
    settings.dither = static_cast<core::DitherMode>(std::clamp(
        conversion.value(QStringLiteral("dither")).toInt(2), 0, 7));
    settings.perceptualColorMatching = conversion.value(
        QStringLiteral("perceptualColorMatching")).toBool(false);
    settings.perceptualRedWeight = std::clamp(
        conversion.value(QStringLiteral("perceptualRedWeight")).toInt(30) / 100.0,
        0.0, 1.0);
    settings.perceptualGreenWeight = std::clamp(
        conversion.value(QStringLiteral("perceptualGreenWeight")).toInt(52) / 100.0,
        0.0, 1.0);
    settings.perceptualBlueWeight = std::clamp(
        conversion.value(QStringLiteral("perceptualBlueWeight")).toInt(18) / 100.0,
        0.0, 1.0);
    settings.stretchHistogram = conversion.value(
        QStringLiteral("stretchHistogram")).toBool(false);
    settings.maximumColorShiftPercent = std::clamp(
        conversion.value(QStringLiteral("maximumColorShift")).toDouble(1.0), 0.0, 100.0);
    settings.gamma = std::clamp(
        conversion.value(QStringLiteral("gamma")).toDouble(1.0), 0.1, 5.0);
    settings.lumaEmphasis = std::clamp(
        conversion.value(QStringLiteral("lumaEmphasis")).toDouble(1.2), 0.0, 10.0);
    settings.maximumMulticolorDifferencePercent = std::clamp(
        conversion.value(QStringLiteral("maximumMulticolorDifference")).toInt(95), 0, 100);
    settings.orderedDitherBrightness = std::clamp(
        conversion.value(QStringLiteral("orderedBrightness")).toInt(0), 0, 16);
    settings.errorAccumulation = static_cast<core::ErrorAccumulationMode>(std::clamp(
        conversion.value(QStringLiteral("errorAccumulation")).toInt(1), 0, 1));
    settings.orderedDitherMapSize =
        conversion.value(QStringLiteral("orderedDitherMapSize")).toInt(2) == 4
        ? core::OrderedDitherMapSize::FourByFour
        : core::OrderedDitherMapSize::TwoByTwo;
    settings.errorDistribution = {
        static_cast<std::uint8_t>(std::clamp(conversion.value(
            QStringLiteral("errorDownLeft")).toInt(2), 0, 16)),
        static_cast<std::uint8_t>(std::clamp(conversion.value(
            QStringLiteral("errorDown")).toInt(2), 0, 16)),
        static_cast<std::uint8_t>(std::clamp(conversion.value(
            QStringLiteral("errorDownRight")).toInt(2), 0, 16)),
        static_cast<std::uint8_t>(std::clamp(conversion.value(
            QStringLiteral("errorRight")).toInt(2), 0, 16)),
        static_cast<std::uint8_t>(std::clamp(conversion.value(
            QStringLiteral("errorFarRight")).toInt(1), 0, 16)),
        static_cast<std::uint8_t>(std::clamp(conversion.value(
            QStringLiteral("errorDownTwo")).toInt(1), 0, 16)),
    };
    settings.paletteSelection = static_cast<core::PaletteSelectionMode>(std::clamp(
        conversion.value(QStringLiteral("paletteSelection")).toInt(0), 0, 1));
    settings.scanlineStaticColorCount = std::clamp(
        conversion.value(QStringLiteral("scanlineStaticColorCount")).toInt(0), 0, 14);
    settings.scanlineRegion1 = conversion.value(QStringLiteral("scanlineRegion1")).toBool(true);
    settings.scanlineRegion2 = conversion.value(QStringLiteral("scanlineRegion2")).toBool(true);
    settings.scanlineRegion3 = conversion.value(QStringLiteral("scanlineRegion3")).toBool(true);

    pipeline.scalingFilter = static_cast<core::ScalingFilter>(std::clamp(
        conversion.value(QStringLiteral("scalingFilter")).toInt(4), 0, 5));
    pipeline.fillMode = static_cast<core::ImageFillMode>(std::clamp(
        conversion.value(QStringLiteral("fillMode")).toInt(0), 0, 3));
    pipeline.horizontalOffset = std::clamp(
        conversion.value(QStringLiteral("horizontalOffset")).toInt(0), -256, 256);
    pipeline.verticalOffset = std::clamp(
        conversion.value(QStringLiteral("verticalOffset")).toInt(0), -192, 192);
    pipeline.powerPaintFraming = conversion.value(
        QStringLiteral("powerPaintFraming")).toBool(false);
    const QColor background(conversion.value(QStringLiteral("backgroundColor"))
                                .toString(QStringLiteral("#000000")));
    if (!background.isValid()) {
        error = QStringLiteral("Recipe contains an invalid background color.");
        return false;
    }
    pipeline.backgroundColor = {static_cast<std::uint8_t>(background.red()),
                                static_cast<std::uint8_t>(background.green()),
                                static_cast<std::uint8_t>(background.blue())};
    const QJsonArray palette = conversion.value(QStringLiteral("workingPalette")).toArray();
    if (!palette.isEmpty()) {
        if (palette.size() != 15) {
            error = QStringLiteral("Recipe working palette must contain 15 colors.");
            return false;
        }
        for (const auto value : palette) {
            const QColor color(value.toString());
            if (!color.isValid()) {
                error = QStringLiteral("Recipe working palette contains an invalid color.");
                return false;
            }
            pipeline.workingPalette.push_back(
                {static_cast<std::uint8_t>(color.red()),
                 static_cast<std::uint8_t>(color.green()),
                 static_cast<std::uint8_t>(color.blue())});
        }
    }
    exportFormat = std::clamp(
        conversion.value(QStringLiteral("exportFormat")).toInt(0), 0, 8);
    sourcePath = root.value(QStringLiteral("source")).toObject()
                     .value(QStringLiteral("path")).toString();
    if (!sourcePath.isEmpty() && QFileInfo(sourcePath).isRelative()) {
        sourcePath = QFileInfo(path).dir().absoluteFilePath(sourcePath);
    }
    return true;
}

int reportFailure(bool json,
                  ExitCode exitCode,
                  const QString& code,
                  const QString& message,
                  QJsonObject details = {})
{
    if (json) {
        QJsonObject result{
            {QStringLiteral("status"), QStringLiteral("error")},
            {QStringLiteral("code"), code},
            {QStringLiteral("message"), message},
            {QStringLiteral("exitCode"), static_cast<int>(exitCode)},
        };
        if (!details.isEmpty()) result.insert(QStringLiteral("details"), details);
        QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Compact) << '\n';
    } else {
        QTextStream(stderr) << "retrovdp-cli: " << message << '\n';
    }
    return static_cast<int>(exitCode);
}

bool applyPreset(const QString& name,
                 core::ConversionSettings& settings,
                 appsupport::ConversionPipelineOptions& pipeline)
{
    if (name == QStringLiteral("balanced")) {
        settings = {};
        pipeline.scalingFilter = core::ScalingFilter::Bilinear;
    } else if (name == QStringLiteral("crisp-pixel-art")
               || name == QStringLiteral("crisp")) {
        settings.dither = core::DitherMode::None;
        settings.stretchHistogram = false;
        settings.maximumColorShiftPercent = 0.0;
        pipeline.scalingFilter = core::ScalingFilter::None;
    } else if (name == QStringLiteral("smooth-photograph")
               || name == QStringLiteral("smooth")) {
        settings.dither = core::DitherMode::Atkinson;
        settings.stretchHistogram = true;
        settings.maximumColorShiftPercent = 2.0;
        pipeline.scalingFilter = core::ScalingFilter::Blackman;
    } else if (name == QStringLiteral("ordered-retro")
               || name == QStringLiteral("ordered")) {
        settings.dither = core::DitherMode::Ordered;
        settings.stretchHistogram = false;
        settings.orderedDitherBrightness = 0;
        pipeline.scalingFilter = core::ScalingFilter::Bilinear;
    } else {
        return false;
    }
    if (const auto configuration = core::ditherConfiguration(settings.dither)) {
        settings.errorDistribution = configuration->kernel;
    }
    return true;
}

QString safeBaseName(const QString& inputPath, const QString& requested)
{
    QString baseName = requested.isEmpty() ? QFileInfo(inputPath).completeBaseName() : requested;
    baseName.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")),
                     QStringLiteral("_"));
    return baseName;
}

QString firstConversionError(const core::ConversionResult& result)
{
    for (const auto& diagnostic : result.diagnostics) {
        if (diagnostic.severity == core::DiagnosticSeverity::Error) {
            return QString::fromStdString(diagnostic.message);
        }
    }
    return QStringLiteral("Conversion failed without an error diagnostic.");
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("retrovdp-cli"));
    application.setApplicationVersion(QStringLiteral(RETROVDP_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
        "Headless conversion and export for RetroVDP Studio target profiles."));
    const QCommandLineOption helpOption = parser.addHelpOption();
    const QCommandLineOption versionOption = parser.addVersionOption();
    const QCommandLineOption inputOption(
        {QStringLiteral("i"), QStringLiteral("input")},
        QStringLiteral("Source image path."), QStringLiteral("file"));
    const QCommandLineOption recipeOption(
        QStringLiteral("recipe"),
        QStringLiteral("Load GUI-compatible conversion settings from a .rvdp.json recipe."),
        QStringLiteral("file"));
    const QCommandLineOption outputOption(
        {QStringLiteral("o"), QStringLiteral("output")},
        QStringLiteral("Destination directory."), QStringLiteral("directory"));
    const QCommandLineOption modeOption(
        {QStringLiteral("m"), QStringLiteral("mode")},
        QStringLiteral("Conversion mode: registered TMS9918A/F18A and Yamaha SCREEN modes; mode-4-sms-192, mode-4-sms-224, mode-4-sms-240-pal; or mode-5-genesis-h32, mode-5-genesis-h40, mode-5-genesis-h32-pal, and mode-5-genesis-h40-pal."),
        QStringLiteral("name"),
        QStringLiteral("bitmap-9918a"));
    const QCommandLineOption targetOption(
        {QStringLiteral("t"), QStringLiteral("target")},
        QStringLiteral("Target VDP profile: tms9918a, f18a, v9938, v9958, sega-sms-vdp, or sega-genesis-vdp."),
        QStringLiteral("name"));
    const QCommandLineOption presetOption(
        {QStringLiteral("p"), QStringLiteral("preset")},
        QStringLiteral("Preset: balanced, crisp-pixel-art, smooth-photograph, or ordered-retro."),
        QStringLiteral("name"), QStringLiteral("balanced"));
    const QCommandLineOption formatOption(
        {QStringLiteral("f"), QStringLiteral("format")},
        QStringLiteral("Export: tifiles, v9t9, raw, rle, msx-sc2, coleco-cvpaint, adam-powerpaint, adam-hgr, or png."),
        QStringLiteral("name"), QStringLiteral("tifiles"));
    const QCommandLineOption baseNameOption(
        {QStringLiteral("b"), QStringLiteral("base-name")},
        QStringLiteral("Safe output base name; defaults to the source file name."),
        QStringLiteral("name"));
    const QCommandLineOption overwriteOption(
        QStringLiteral("overwrite"), QStringLiteral("Replace existing output files."));
    const QCommandLineOption jsonOption(
        QStringLiteral("json"), QStringLiteral("Write one machine-readable JSON result."));
    parser.addOptions({inputOption, recipeOption, outputOption, targetOption, modeOption,
                       presetOption, formatOption, baseNameOption, overwriteOption, jsonOption});

    const QStringList arguments = application.arguments();
    const bool wantsJson = arguments.contains(QStringLiteral("--json"));
    if (!parser.parse(arguments)) {
        return reportFailure(wantsJson, ExitCode::Usage, QStringLiteral("invalid-arguments"),
                             parser.errorText());
    }
    if (parser.isSet(helpOption)) {
        QTextStream(stdout) << parser.helpText();
        return static_cast<int>(ExitCode::Success);
    }
    if (parser.isSet(versionOption)) {
        QTextStream(stdout) << application.applicationName() << ' '
                            << application.applicationVersion() << '\n';
        return static_cast<int>(ExitCode::Success);
    }

    core::ConversionSettings settings;
    appsupport::ConversionPipelineOptions pipeline;
    int recipeExportFormat = 0;
    QString recipeSourcePath;
    if (parser.isSet(recipeOption)) {
        QString recipeError;
        if (!loadRecipe(parser.value(recipeOption), settings, pipeline, recipeExportFormat,
                        recipeSourcePath, recipeError)) {
            return reportFailure(wantsJson, ExitCode::Usage,
                                 QStringLiteral("invalid-recipe"), recipeError);
        }
    }

    const QString inputPath = parser.value(inputOption).isEmpty()
        ? recipeSourcePath : parser.value(inputOption);
    const QString outputPath = parser.value(outputOption);
    if (inputPath.isEmpty() || outputPath.isEmpty()) {
        return reportFailure(
            wantsJson, ExitCode::Usage, QStringLiteral("missing-required-option"),
            QStringLiteral("--output is required, and input must come from --input or the recipe."));
    }

    const QString presetName = parser.isSet(recipeOption) && !parser.isSet(presetOption)
        ? QStringLiteral("recipe") : parser.value(presetOption).toLower();
    if ((!parser.isSet(recipeOption) || parser.isSet(presetOption))
        && !applyPreset(presetName, settings, pipeline)) {
        return reportFailure(wantsJson, ExitCode::Usage, QStringLiteral("unknown-preset"),
                             QStringLiteral("Unknown preset: %1").arg(presetName));
    }
    if (!parser.isSet(recipeOption) || parser.isSet(modeOption)) {
        const QString requestedMode = parser.value(modeOption).toLower();
        const auto mode = core::conversionMode(requestedMode.toStdString());
        if (!mode) {
            return reportFailure(wantsJson, ExitCode::Usage, QStringLiteral("unknown-mode"),
                                 QStringLiteral("Unknown conversion mode: %1")
                                     .arg(requestedMode));
        }
        settings.mode = *mode;
    }
    if (parser.isSet(targetOption)) {
        const QString requestedTarget = parser.value(targetOption).toLower();
        const auto target = core::targetProfileId(requestedTarget.toStdString());
        if (!target) {
            return reportFailure(wantsJson, ExitCode::Usage,
                                 QStringLiteral("unknown-target"),
                                 QStringLiteral("Unknown target VDP: %1")
                                     .arg(requestedTarget));
        }
        if (!core::supportsConversionMode(*target, settings.mode)) {
            return reportFailure(
                wantsJson, ExitCode::Usage,
                QStringLiteral("unsupported-target-mode"),
                QStringLiteral("The selected target VDP does not support this conversion mode."));
        }
        settings.targetProfile = *target;
    } else {
        settings.targetProfile = core::effectiveTargetProfile(
            settings.targetProfile, settings.mode);
    }
    const auto& selectedMode = core::displayMode(settings.mode);
    const QString modeName = QString::fromLatin1(
        selectedMode.stableId.data(), static_cast<qsizetype>(selectedMode.stableId.size()));
    const auto& selectedTarget = core::targetProfile(settings.targetProfile);
    const QString targetName = QString::fromLatin1(
        selectedTarget.stableId.data(),
        static_cast<qsizetype>(selectedTarget.stableId.size()));

    std::optional<formats::ExportFormat> format;
    if (parser.isSet(recipeOption) && !parser.isSet(formatOption)) {
        format = formatChoices[static_cast<std::size_t>(recipeExportFormat)].value;
    } else {
        const QString requestedFormat = parser.value(formatOption).toLower();
        format = choiceValue<FormatChoice, formatChoices.size(), formats::ExportFormat>(
            formatChoices, requestedFormat);
        if (!format) {
            return reportFailure(wantsJson, ExitCode::Usage, QStringLiteral("unknown-format"),
                                 QStringLiteral("Unknown export format: %1")
                                     .arg(requestedFormat));
        }
    }
    const QString formatName = choiceName(formatChoices, *format);

    auto loaded = imageio::loadImageFile(inputPath);
    if (!loaded) {
        return reportFailure(wantsJson, ExitCode::Input, QStringLiteral("input-failed"),
                             loaded.error.isEmpty()
                                 ? QStringLiteral("The source image could not be loaded.")
                                 : loaded.error);
    }
    auto source = std::make_shared<const core::RgbImage>(std::move(*loaded.image));
    const core::ConversionRequest conversionRequest{
        .source = std::move(source),
        .settings = settings,
        .generation = 1,
    };
    auto converted = appsupport::runConversion(conversionRequest, std::move(pipeline));
    if (!converted.succeeded() || !converted.preview || !converted.target) {
        return reportFailure(wantsJson, ExitCode::Conversion,
                             QStringLiteral("conversion-failed"),
                             firstConversionError(converted));
    }

    const QString baseName = safeBaseName(inputPath, parser.value(baseNameOption));
    if (baseName.isEmpty()) {
        return reportFailure(wantsJson, ExitCode::Usage, QStringLiteral("invalid-base-name"),
                             QStringLiteral("The output base name contains no usable characters."));
    }
    const formats::ExportRequest exportRequest{
        .format = *format,
        .baseName = baseName.toStdString(),
        .target = &*converted.target,
        .preview = &*converted.preview,
    };
    auto manifest = *format == formats::ExportFormat::Png
        ? imageio::generatePngExport(exportRequest)
        : formats::generateExport(exportRequest);
    if (!manifest) {
        return reportFailure(wantsJson, ExitCode::Export,
                             QStringLiteral("export-unavailable"),
                             QString::fromStdString(manifest.message));
    }

    const auto written = imageio::writeExportManifest(
        outputPath, manifest, parser.isSet(overwriteOption));
    if (!written) {
        QJsonArray conflicts;
        for (const QString& path : written.conflicts) conflicts.push_back(path);
        QJsonObject details;
        if (!conflicts.isEmpty()) details.insert(QStringLiteral("conflicts"), conflicts);
        const QString code = written.status == imageio::ExportWriteStatus::WouldOverwrite
            ? QStringLiteral("output-conflict")
            : QStringLiteral("write-failed");
        return reportFailure(wantsJson, ExitCode::Write, code, written.error, details);
    }

    if (wantsJson) {
        QJsonArray files;
        for (std::size_t index = 0; index < manifest.files.size(); ++index) {
            files.push_back(QJsonObject{
                {QStringLiteral("path"), written.paths[static_cast<qsizetype>(index)]},
                {QStringLiteral("bytes"),
                 static_cast<qint64>(manifest.files[index].bytes.size())},
            });
        }
        QJsonArray warnings;
        for (const auto& warning : manifest.warnings) {
            warnings.push_back(QString::fromStdString(warning));
        }
        const QJsonObject result{
            {QStringLiteral("status"), QStringLiteral("ok")},
            {QStringLiteral("input"), QFileInfo(inputPath).absoluteFilePath()},
            {QStringLiteral("output"), QDir(outputPath).absolutePath()},
            {QStringLiteral("target"), targetName},
            {QStringLiteral("mode"), modeName},
            {QStringLiteral("preset"), presetName},
            {QStringLiteral("format"), formatName},
            {QStringLiteral("files"), files},
            {QStringLiteral("warnings"), warnings},
        };
        QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Compact) << '\n';
    } else {
        QTextStream output(stdout);
        output << "Converted " << QFileInfo(inputPath).fileName() << " for "
               << QString::fromLatin1(selectedTarget.displayName.data(),
                                      static_cast<qsizetype>(
                                          selectedTarget.displayName.size()))
               << " using " << modeName
               << " and wrote " << written.paths.size() << " file(s):\n";
        for (const QString& path : written.paths) output << "  " << path << '\n';
        for (const auto& warning : manifest.warnings) {
            output << "Warning: " << QString::fromStdString(warning) << '\n';
        }
    }
    return static_cast<int>(ExitCode::Success);
}
