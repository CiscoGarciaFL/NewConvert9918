#pragma once

#include "retrovdp/core/ConversionSettings.hpp"
#include "retrovdp/core/ConversionTypes.hpp"

#include <string>
#include <vector>

namespace retrovdp::core {

struct ValidationIssue {
    std::string field;
    std::string message;
};

[[nodiscard]] std::vector<ValidationIssue>
validate(const ConversionSettings& settings);

[[nodiscard]] std::vector<ValidationIssue>
validate(const ConversionRequest& request);

} // namespace retrovdp::core
