#pragma once

#include "newconvert9918/core/RgbImage.hpp"
#include "newconvert9918/core/TargetData.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace newconvert9918::formats {

enum class ExportFormat : std::uint8_t {
    Raw,
    Rle,
    TiFiles,
    V9t9,
    MsxScreen2,
    ColecoCvPaint,
    AdamPowerPaint,
    AdamHgr,
    ColecoVisionRom,
    ExtendedBasicProgram,
    ExtendedBasicRleProgram,
    Png,
};

enum class ExportError : std::uint8_t {
    None,
    MissingTarget,
    MissingPreview,
    InvalidTargetTables,
    InvalidBaseName,
    UnsupportedForConversionMode,
    MissingLoaderTemplate,
    InvalidLoaderTemplate,
    OutputTooLarge,
    AdapterRequired,
    EncodingFailed,
};

struct GeneratedFile {
    std::string fileName;
    std::vector<std::uint8_t> bytes;
};

struct GeneratedFileManifest {
    ExportFormat format{ExportFormat::Raw};
    std::vector<GeneratedFile> files;
    std::vector<std::string> warnings;
    ExportError error{ExportError::None};
    std::string message;

    [[nodiscard]] explicit operator bool() const { return error == ExportError::None; }
};

// Loader templates keep machine code outside the portable writer. This lets a
// distributor provide an independently built or otherwise licensed template
// without coupling export logic to a particular toolchain or opaque resource.
struct LegacyLoaderTemplates {
    std::span<const std::uint8_t> colecoVisionRom;
    std::span<const std::uint8_t> extendedBasicProgram;
};

struct ExportRequest {
    ExportFormat format{ExportFormat::Raw};
    std::string baseName;
    const core::TargetMemoryImage* target{};
    const core::RgbImage* preview{};
    LegacyLoaderTemplates loaders{};
};

[[nodiscard]] bool isExportApplicable(ExportFormat format, core::ConversionMode mode);
[[nodiscard]] GeneratedFileManifest generateExport(const ExportRequest& request);

// Public for independent decoder/encoder tests and future streaming frontends.
[[nodiscard]] std::vector<std::uint8_t>
encodeLegacyRle(std::span<const std::uint8_t> input);

} // namespace newconvert9918::formats
