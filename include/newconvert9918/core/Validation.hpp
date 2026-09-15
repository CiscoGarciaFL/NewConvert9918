#pragma once

#include "newconvert9918/core/ConversionSettings.hpp"

#include <string>
#include <vector>

namespace newconvert9918::core {

struct ValidationIssue {
    std::string field;
    std::string message;
};

[[nodiscard]] std::vector<ValidationIssue>
validate(const ConversionSettings& settings);

} // namespace newconvert9918::core

