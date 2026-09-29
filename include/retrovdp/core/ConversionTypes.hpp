#pragma once

#include "retrovdp/core/ConversionSettings.hpp"
#include "retrovdp/core/RgbImage.hpp"
#include "retrovdp/core/TargetData.hpp"

#include <cstdint>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace retrovdp::core {

using ConversionProgressCallback =
    std::function<void(const RgbImage& preview,
                       std::uint32_t completedRows,
                       std::uint32_t totalRows)>;

class CancellationToken final {
public:
    CancellationToken() = default;

    [[nodiscard]] bool isCancellationRequested() const;

private:
    explicit CancellationToken(std::shared_ptr<const std::atomic_bool> requested);

    std::shared_ptr<const std::atomic_bool> requested_;

    friend class CancellationSource;
};

class CancellationSource final {
public:
    CancellationSource();

    [[nodiscard]] CancellationToken token() const;
    void requestCancellation() const;

private:
    std::shared_ptr<std::atomic_bool> requested_;
};

struct ConversionRequest {
    std::shared_ptr<const RgbImage> source;
    ConversionSettings settings;
    std::uint64_t generation{};
    CancellationToken cancellation;
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
    std::optional<TargetMemoryImage> target;
    std::uint64_t generation{};

    [[nodiscard]] bool hasErrors() const;
    [[nodiscard]] bool succeeded() const;
};

} // namespace retrovdp::core
