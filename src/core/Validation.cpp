#include "newconvert9918/core/Validation.hpp"

#include <cmath>

namespace newconvert9918::core {

std::vector<ValidationIssue> validate(const ConversionSettings& settings)
{
    std::vector<ValidationIssue> issues;

    switch (settings.dither) {
    case DitherMode::None:
    case DitherMode::FloydSteinberg:
    case DitherMode::Atkinson:
    case DitherMode::Pattern:
    case DitherMode::Diagonal:
    case DitherMode::Ordered:
    case DitherMode::OrderedWithError:
        break;
    default:
        issues.push_back({"dither", "Dither mode is not supported."});
        break;
    }

    switch (settings.orderedDitherMapSize) {
    case OrderedDitherMapSize::TwoByTwo:
    case OrderedDitherMapSize::FourByFour:
        break;
    default:
        issues.push_back({
            "orderedDitherMapSize",
            "Ordered dither map size must be 2x2 or 4x4.",
        });
        break;
    }

    if (settings.orderedDitherBrightness < 0
        || settings.orderedDitherBrightness > 16) {
        issues.push_back({
            "orderedDitherBrightness",
            "Ordered dither brightness must be between 0 and 16.",
        });
    }

    switch (settings.errorAccumulation) {
    case ErrorAccumulationMode::Average:
    case ErrorAccumulationMode::Accumulate:
        break;
    default:
        issues.push_back({
            "errorAccumulation",
            "Error accumulation mode is not supported.",
        });
        break;
    }

    if (settings.targetWidth <= 0) {
        issues.push_back({"targetWidth", "Target width must be positive."});
    }

    if (settings.targetHeight <= 0) {
        issues.push_back({"targetHeight", "Target height must be positive."});
    }

    if (!std::isfinite(settings.gamma) || settings.gamma <= 0.0) {
        issues.push_back({"gamma", "Gamma must be greater than zero."});
    }

    if (!std::isfinite(settings.maximumColorShiftPercent)
        || settings.maximumColorShiftPercent < 0.0
        || settings.maximumColorShiftPercent > 100.0) {
        issues.push_back({
            "maximumColorShiftPercent",
            "Maximum color shift must be between 0 and 100 percent.",
        });
    }

    if (settings.maximumMulticolorDifferencePercent < 1
        || settings.maximumMulticolorDifferencePercent > 100) {
        issues.push_back({
            "maximumMulticolorDifferencePercent",
            "Maximum multicolor difference must be between 1 and 100 percent.",
        });
    }

    if (!std::isfinite(settings.perceptualRedWeight)
        || settings.perceptualRedWeight < 0.0) {
        issues.push_back({
            "perceptualRedWeight",
            "The perceptual red weight must be finite and nonnegative.",
        });
    }

    if (!std::isfinite(settings.perceptualGreenWeight)
        || settings.perceptualGreenWeight < 0.0) {
        issues.push_back({
            "perceptualGreenWeight",
            "The perceptual green weight must be finite and nonnegative.",
        });
    }

    if (!std::isfinite(settings.perceptualBlueWeight)
        || settings.perceptualBlueWeight < 0.0) {
        issues.push_back({
            "perceptualBlueWeight",
            "The perceptual blue weight must be finite and nonnegative.",
        });
    }

    if (settings.perceptualRedWeight == 0.0
        && settings.perceptualGreenWeight == 0.0
        && settings.perceptualBlueWeight == 0.0) {
        issues.push_back({
            "perceptualColorWeights",
            "At least one perceptual color weight must be greater than zero.",
        });
    }

    if (!std::isfinite(settings.lumaEmphasis) || settings.lumaEmphasis < 0.0) {
        issues.push_back({
            "lumaEmphasis",
            "Luma emphasis must be finite and nonnegative.",
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
