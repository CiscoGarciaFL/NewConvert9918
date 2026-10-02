#pragma once

#include "retrovdp/core/ConversionTypes.hpp"

#include <span>
#include <vector>

namespace retrovdp::core {

// The application supplies prepared source pixels; this registry owns the
// target compiler selection. A mode is therefore added by registering its
// compiler here rather than by extending an application-wide dispatch switch.
[[nodiscard]] ConversionResult
compileRegisteredConversion(const RgbImage& source,
                            const ConversionSettings& settings,
                            std::span<const RgbColor> workingPalette = {},
                            CancellationToken cancellation = {},
                            ConversionProgressCallback progress = {});

} // namespace retrovdp::core
