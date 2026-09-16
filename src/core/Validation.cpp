#include "newconvert9918/core/Validation.hpp"

namespace newconvert9918::core {

std::vector<ValidationIssue> validate(const ConversionSettings& settings)
{
    std::vector<ValidationIssue> issues;

    if (settings.targetWidth <= 0) {
        issues.push_back({"targetWidth", "Target width must be positive."});
    }

    if (settings.targetHeight <= 0) {
        issues.push_back({"targetHeight", "Target height must be positive."});
    }

    if (settings.gamma <= 0.0) {
        issues.push_back({"gamma", "Gamma must be greater than zero."});
    }

    if (settings.maximumColorShiftPercent < 0.0
        || settings.maximumColorShiftPercent > 100.0) {
        issues.push_back({
            "maximumColorShiftPercent",
            "Maximum color shift must be between 0 and 100 percent.",
        });
    }

    return issues;
}

std::vector<ValidationIssue> validate(const ConversionRequest& request)
{
    std::vector<ValidationIssue> issues;
    if (!request.source) {
        issues.push_back({"source", "A source image is required."});
    }

    std::vector<ValidationIssue> settingsIssues = validate(request.settings);
    issues.insert(issues.end(), settingsIssues.begin(), settingsIssues.end());
    return issues;
}

} // namespace newconvert9918::core
