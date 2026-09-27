#pragma once

#include "newconvert9918/core/ConversionTypes.hpp"
#include "newconvert9918/core/ImageTransform.hpp"

#include <vector>

namespace newconvert9918::appsupport {

struct ConversionPipelineOptions {
    core::ScalingFilter scalingFilter{core::ScalingFilter::Bilinear};
    core::ImageFillMode fillMode{core::ImageFillMode::Fit};
    int horizontalOffset{};
    int verticalOffset{};
    core::RgbColor backgroundColor{};
    std::vector<core::RgbColor> workingPalette;
    bool powerPaintFraming{};
    bool sourceAlreadyFramed{};
    core::ConversionProgressCallback progress;
};

[[nodiscard]] std::vector<core::RgbColor> defaultWorkingPalette();

[[nodiscard]] core::ConversionResult
runConversion(const core::ConversionRequest& request,
              ConversionPipelineOptions options = {});

} // namespace newconvert9918::appsupport
