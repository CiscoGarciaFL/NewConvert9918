#pragma once

#include "retrovdp/core/ConversionTypes.hpp"

namespace retrovdp::core {

// Compiles Sega Master System Mode 4 screen images into 4-bit planar pattern
// data, a 32-column name table, 32 bytes of CRAM, and initial VDP registers.
// The source dimensions must match the registered 192, 224, or 240-line mode.
[[nodiscard]] ConversionResult
convertSegaSmsMode4(const RgbImage& source,
                    const ConversionSettings& settings,
                    CancellationToken cancellation = {},
                    ConversionProgressCallback progress = {});

} // namespace retrovdp::core
