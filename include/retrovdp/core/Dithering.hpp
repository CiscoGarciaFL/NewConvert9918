#pragma once

#include "retrovdp/core/ColorMath.hpp"
#include "retrovdp/core/ConversionSettings.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace retrovdp::core {

struct DitherConfiguration {
    bool ordered{};
    bool distributeError{};
    ErrorDistributionKernel kernel{};
};

[[nodiscard]] std::optional<DitherConfiguration>
ditherConfiguration(DitherMode mode,
                    ErrorDistributionKernel customKernel = {});

[[nodiscard]] std::optional<double>
orderedDitherThreshold(OrderedDitherMapSize size,
                       std::uint32_t x,
                       std::uint32_t y,
                       int brightness);

[[nodiscard]] std::optional<RgbSample>
applyOrderedDither(RgbSample input,
                   std::uint32_t x,
                   std::uint32_t y,
                   OrderedDitherMapSize size,
                   int brightness);

enum class ErrorDiffusionBufferError : std::uint8_t {
    None,
    ZeroDimension,
    PixelLimitExceeded,
    SizeOverflow,
};

class ErrorDiffusionBuffer final {
public:
    [[nodiscard]] static std::optional<ErrorDiffusionBuffer>
    create(std::uint32_t width,
           std::uint32_t height,
           std::size_t maximumPixels = 4U * 1024U * 1024U,
           ErrorDiffusionBufferError* error = nullptr);

    [[nodiscard]] std::uint32_t width() const { return width_; }
    [[nodiscard]] std::uint32_t height() const { return height_; }
    [[nodiscard]] RgbSample errorAt(std::uint32_t x, std::uint32_t y) const;
    [[nodiscard]] RgbSample adjustedSample(RgbSample source,
                                           std::uint32_t x,
                                           std::uint32_t y,
                                           ErrorAccumulationMode mode) const;

    void distribute(std::uint32_t x,
                    std::uint32_t y,
                    RgbSample quantizationError,
                    const ErrorDistributionKernel& kernel);

private:
    ErrorDiffusionBuffer(std::uint32_t width,
                         std::uint32_t height,
                         std::vector<RgbSample> errors);

    [[nodiscard]] std::size_t index(std::uint32_t x, std::uint32_t y) const;
    void add(std::uint32_t x,
             std::uint32_t y,
             RgbSample error,
             std::uint8_t weight);

    std::uint32_t width_{};
    std::uint32_t height_{};
    std::vector<RgbSample> errors_;
};

} // namespace retrovdp::core
