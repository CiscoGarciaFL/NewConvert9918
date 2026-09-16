#pragma once

#include "newconvert9918/core/ConversionSettings.hpp"
#include "newconvert9918/core/ConversionTypes.hpp"

#include <string>
#include <vector>

namespace newconvert9918::core {

struct ValidationIssue {
    std::string field;
    std::string message;
};

[[nodiscard]] std::vector<ValidationIssue>
validate(const ConversionSettings& settings);

[[nodiscard]] std::vector<ValidationIssue>
validate(const ConversionRequest& request);

} // namespace newconvert9918::core
