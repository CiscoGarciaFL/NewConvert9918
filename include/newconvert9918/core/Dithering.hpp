#pragma once

#include "newconvert9918/core/ColorMath.hpp"
#include "newconvert9918/core/ConversionSettings.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace newconvert9918::core {

struct ErrorDistributionKernel {
    std::uint8_t downLeft{};
    std::uint8_t down{};
    std::uint8_t downRight{};
    std::uint8_t right{};
    std::uint8_t farRight{};
    std::uint8_t downTwo{};

    [[nodiscard]] constexpr int totalWeight() const
    {
        return downLeft + down + downRight + right + farRight + downTwo;
    }
};

struct DitherConfiguration {
    bool ordered{};
    bool distributeError{};
    ErrorDistributionKernel kernel{};
};

[[nodiscard]] std::optional<DitherConfiguration>
ditherConfiguration(DitherMode mode);

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

} // namespace newconvert9918::core
