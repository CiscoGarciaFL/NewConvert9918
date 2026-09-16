#include "newconvert9918/core/ConversionJobController.hpp"

#include <utility>

namespace newconvert9918::core {

ConversionRequest ConversionJobController::begin(
    std::shared_ptr<const RgbImage> source,
    ConversionSettings settings)
{
    std::scoped_lock lock(mutex_);
    if (currentCancellation_) {
        currentCancellation_->requestCancellation();
    }
    currentCancellation_.emplace();
    currentGeneration_ = nextGeneration_++;
    return {
        .source = std::move(source),
        .settings = settings,
        .generation = currentGeneration_,
        .cancellation = currentCancellation_->token(),
    };
}

void ConversionJobController::cancelCurrent()
{
    std::scoped_lock lock(mutex_);
    if (currentCancellation_) {
        currentCancellation_->requestCancellation();
    }
}

ConversionResult ConversionJobController::finalize(
    const ConversionRequest& request,
    ConversionResult result) const
{
    result.generation = request.generation;
    if (request.cancellation.isCancellationRequested()) {
        result.status = ConversionStatus::Cancelled;
        result.preview.reset();
        result.target.reset();
        result.diagnostics = {{
            DiagnosticSeverity::Information,
            "conversion-cancelled",
            "The conversion was superseded or cancelled before publication.",
        }};
    }
    return result;
}

bool ConversionJobController::isCurrent(std::uint64_t generation) const
{
    std::scoped_lock lock(mutex_);
    return generation != 0 && generation == currentGeneration_;
}

bool ConversionJobController::accepts(const ConversionResult& result) const
{
    return result.status != ConversionStatus::Cancelled
        && isCurrent(result.generation);
}

} // namespace newconvert9918::core
