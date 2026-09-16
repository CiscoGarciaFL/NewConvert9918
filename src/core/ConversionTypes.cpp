#include "newconvert9918/core/ConversionTypes.hpp"

#include <algorithm>

namespace newconvert9918::core {

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
