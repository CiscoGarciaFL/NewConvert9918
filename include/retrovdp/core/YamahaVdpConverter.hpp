#pragma once

#include "retrovdp/core/ConversionTypes.hpp"

namespace retrovdp::core {

// Compiles the native V9938/V9958 bitmap modes. The source geometry must
// match the registered display-mode geometry; the returned framebuffer is a
// hardware-order VRAM page and palette data is in Yamaha port-write order.
[[nodiscard]] ConversionResult
convertYamahaBitmap(const RgbImage& source,
                    const ConversionSettings& settings,
                    CancellationToken cancellation = {},
                    ConversionProgressCallback progress = {});

} // namespace retrovdp::core
