#pragma once

#include "newconvert9918/formats/Export.hpp"

#include <QString>
#include <QStringList>

#include <cstdint>

namespace newconvert9918::imageio {

enum class ExportWriteStatus : std::uint8_t {
    Written,
    WouldOverwrite,
    Failed,
};

struct ExportWriteResult {
    ExportWriteStatus status{ExportWriteStatus::Failed};
    QStringList paths;
    QStringList conflicts;
    QString error;

    [[nodiscard]] explicit operator bool() const
    {
        return status == ExportWriteStatus::Written;
    }
};

[[nodiscard]] formats::GeneratedFileManifest
generatePngExport(const formats::ExportRequest& request);

// Performs a complete conflict preflight before writing any file. Callers must
// explicitly opt in after presenting the returned conflicts to the user.
[[nodiscard]] ExportWriteResult writeExportManifest(
    const QString& directory,
    const formats::GeneratedFileManifest& manifest,
    bool overwriteExisting = false);

} // namespace newconvert9918::imageio
