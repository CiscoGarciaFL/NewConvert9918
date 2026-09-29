#pragma once

#include "retrovdp/core/ConversionSettings.hpp"

#include <cstddef>

namespace retrovdp::core {

// Conservative accounting of the large, explicitly owned buffers used by one
// prepared 256x192 conversion. Allocator metadata, stacks, and library/runtime
// working sets are intentionally excluded.
struct ConversionMemoryEstimate {
    std::size_t sourceImageBytes{};
    std::size_t previewImageBytes{};
    std::size_t indexedImageBytes{};
    std::size_t errorDiffusionBytes{};
    std::size_t targetTableBytes{};
    std::size_t paletteBytes{};

    [[nodiscard]] constexpr std::size_t totalBytes() const
    {
        return sourceImageBytes + previewImageBytes + indexedImageBytes
            + errorDiffusionBytes + targetTableBytes + paletteBytes;
    }
};

[[nodiscard]] ConversionMemoryEstimate
estimateConversionMemory(const ConversionSettings& settings);

} // namespace retrovdp::core
