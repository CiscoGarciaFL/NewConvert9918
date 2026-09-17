#include "newconvert9918/imageio/ExportWriter.hpp"

#include "newconvert9918/imageio/ImageLoader.hpp"

#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QImageWriter>
#include <QSaveFile>

#include <algorithm>
#include <cctype>
#include <span>

namespace newconvert9918::imageio {
namespace {

bool endsWithPng(const std::string& value)
{
    if (value.size() < 4U) return false;
    std::string suffix = value.substr(value.size() - 4U);
    std::ranges::transform(suffix, suffix.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return suffix == ".png";
}

bool validFileName(const QString& value)
{
    return !value.isEmpty() && value != QStringLiteral(".") && value != QStringLiteral("..")
        && !value.contains('/') && !value.contains('\\') && !value.contains(QChar::Null);
}

formats::GeneratedFileManifest pngFailure(formats::ExportError error, std::string message)
{
    formats::GeneratedFileManifest result;
    result.format = formats::ExportFormat::Png;
    result.error = error;
    result.message = std::move(message);
    return result;
}

ExportWriteResult writeFailure(QString error, QStringList paths = {})
{
    ExportWriteResult result;
    result.status = ExportWriteStatus::Failed;
    result.paths = std::move(paths);
    result.error = std::move(error);
    return result;
}

} // namespace

formats::GeneratedFileManifest generatePngExport(const formats::ExportRequest& request)
{
    if (request.format != formats::ExportFormat::Png) {
        return pngFailure(formats::ExportError::EncodingFailed,
                          "The Qt PNG adapter only accepts PNG export requests.");
    }
    const QString baseName = QString::fromUtf8(request.baseName);
    if (!validFileName(baseName)) {
        return pngFailure(formats::ExportError::InvalidBaseName,
                          "The export base name must be a single non-empty file name.");
    }
    if (request.preview == nullptr) {
        return pngFailure(formats::ExportError::MissingPreview,
                          "PNG export requires a completed preview image.");
    }

    QByteArray encoded;
    QBuffer buffer(&encoded);
    if (!buffer.open(QIODevice::WriteOnly)) {
        return pngFailure(formats::ExportError::EncodingFailed,
                          "The PNG output buffer could not be opened.");
    }
    QImageWriter writer(&buffer, "png");
    writer.setOptimizedWrite(true);
    if (!writer.write(toQImage(*request.preview))) {
        return pngFailure(formats::ExportError::EncodingFailed,
                          writer.errorString().toStdString());
    }

    std::string fileName = request.baseName;
    if (!endsWithPng(fileName)) fileName += ".png";
    formats::GeneratedFile file{
        .fileName = std::move(fileName),
        .bytes = std::vector<std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(encoded.constData()),
            reinterpret_cast<const std::uint8_t*>(encoded.constData()) + encoded.size()),
    };
    formats::GeneratedFileManifest result;
    result.format = formats::ExportFormat::Png;
    result.files.push_back(std::move(file));
    return result;
}

ExportWriteResult writeExportManifest(const QString& directory,
                                      const formats::GeneratedFileManifest& manifest,
                                      bool overwriteExisting)
{
    if (!manifest) {
        return writeFailure(QString::fromStdString(manifest.message));
    }
    if (manifest.files.empty()) {
        return writeFailure(QStringLiteral("The export manifest contains no files."));
    }

    QDir outputDirectory(directory);
    QStringList paths;
    for (const auto& file : manifest.files) {
        const QString fileName = QString::fromUtf8(file.fileName);
        if (!validFileName(fileName)) {
            return writeFailure(
                QStringLiteral("The export manifest contains an unsafe file name."));
        }
        const QString path = outputDirectory.absoluteFilePath(fileName);
        if (paths.contains(path)) {
            return writeFailure(
                QStringLiteral("The export manifest contains duplicate file names."));
        }
        paths.push_back(path);
    }

    QStringList conflicts;
    for (const QString& path : paths) {
        if (QFileInfo::exists(path)) conflicts.push_back(path);
    }
    if (!conflicts.empty() && !overwriteExisting) {
        ExportWriteResult result;
        result.status = ExportWriteStatus::WouldOverwrite;
        result.paths = std::move(paths);
        result.conflicts = std::move(conflicts);
        result.error = QStringLiteral("One or more export files already exist.");
        return result;
    }
    if (!outputDirectory.exists() && !QDir().mkpath(outputDirectory.absolutePath())) {
        return writeFailure(QStringLiteral("The export directory could not be created."),
                            std::move(paths));
    }

    for (qsizetype index = 0; index < paths.size(); ++index) {
        QSaveFile output(paths[index]);
        if (!output.open(QIODevice::WriteOnly)) {
            return writeFailure(QStringLiteral("Could not open %1: %2")
                                    .arg(paths[index], output.errorString()),
                                paths);
        }
        const auto& bytes = manifest.files[static_cast<std::size_t>(index)].bytes;
        const qint64 written = output.write(
            reinterpret_cast<const char*>(bytes.data()), static_cast<qint64>(bytes.size()));
        if (written != static_cast<qint64>(bytes.size()) || !output.commit()) {
            return writeFailure(QStringLiteral("Could not write %1: %2")
                                    .arg(paths[index], output.errorString()),
                                paths);
        }
    }
    ExportWriteResult result;
    result.status = ExportWriteStatus::Written;
    result.paths = std::move(paths);
    return result;
}

} // namespace newconvert9918::imageio
