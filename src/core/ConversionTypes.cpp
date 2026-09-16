#include "newconvert9918/core/ConversionTypes.hpp"

#include <algorithm>
#include <utility>

namespace newconvert9918::core {

CancellationToken::CancellationToken(
    std::shared_ptr<const std::atomic_bool> requested)
    : requested_(std::move(requested))
{
}

bool CancellationToken::isCancellationRequested() const
{
    return requested_ && requested_->load(std::memory_order_relaxed);
}

CancellationSource::CancellationSource()
    : requested_(std::make_shared<std::atomic_bool>(false))
{
}

CancellationToken CancellationSource::token() const
{
    return CancellationToken(requested_);
}

void CancellationSource::requestCancellation() const
{
    requested_->store(true, std::memory_order_relaxed);
}

bool ConversionResult::hasErrors() const
{
    return std::ranges::any_of(diagnostics, [](const ConversionDiagnostic& diagnostic) {
        return diagnostic.severity == DiagnosticSeverity::Error;
    });
}

bool ConversionResult::succeeded() const
{
    return status == ConversionStatus::Succeeded && !hasErrors();
}

} // namespace newconvert9918::core
