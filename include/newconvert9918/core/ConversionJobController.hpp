#pragma once

#include "newconvert9918/core/ConversionTypes.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>

namespace newconvert9918::core {

// Owns the cancellation/generation state shared by an interactive preview
// producer and consumer. Starting a request invalidates and cancels its
// predecessor. Workers call finalize before publishing their result.
class ConversionJobController final {
public:
    [[nodiscard]] ConversionRequest begin(
        std::shared_ptr<const RgbImage> source,
        ConversionSettings settings);

    void cancelCurrent();

    [[nodiscard]] ConversionResult finalize(
        const ConversionRequest& request,
        ConversionResult result) const;

    [[nodiscard]] bool isCurrent(std::uint64_t generation) const;
    [[nodiscard]] bool accepts(const ConversionResult& result) const;

private:
    mutable std::mutex mutex_;
    std::uint64_t nextGeneration_{1};
    std::uint64_t currentGeneration_{};
    std::optional<CancellationSource> currentCancellation_;
};

} // namespace newconvert9918::core
