#pragma once

#include "retrovdp/core/ConversionTypes.hpp"
#include "retrovdp/core/ImageTransform.hpp"

#include <vector>

namespace retrovdp::appsupport {

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

} // namespace retrovdp::appsupport
