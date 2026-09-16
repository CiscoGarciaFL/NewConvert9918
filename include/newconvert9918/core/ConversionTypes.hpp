#pragma once

#include "newconvert9918/core/ConversionSettings.hpp"
#include "newconvert9918/core/RgbImage.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace newconvert9918::core {

struct ConversionRequest {
    std::shared_ptr<const RgbImage> source;
    ConversionSettings settings;
};

enum class DiagnosticSeverity : std::uint8_t {
    Information,
    Warning,
    Error,
};

struct ConversionDiagnostic {
    DiagnosticSeverity severity{DiagnosticSeverity::Information};
    std::string code;
    std::string message;
};

enum class ConversionStatus : std::uint8_t {
    Succeeded,
    Failed,
    Cancelled,
};

struct ConversionResult {
    ConversionStatus status{ConversionStatus::Failed};
    std::optional<RgbImage> preview;
    std::vector<ConversionDiagnostic> diagnostics;

    [[nodiscard]] bool hasErrors() const;
    [[nodiscard]] bool succeeded() const;
};

} // namespace newconvert9918::core
