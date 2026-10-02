#pragma once

#include "retrovdp/core/ConversionTypes.hpp"

namespace retrovdp::core {

[[nodiscard]] ConversionResult
convertSegaGenesisMode5(const RgbImage& source,
                        const ConversionSettings& settings,
                        CancellationToken cancellation = {},
                        ConversionProgressCallback progress = {});

} // namespace retrovdp::core
