#include "newconvert9918/core/Dithering.hpp"

#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

namespace newconvert9918::core {
namespace {

constexpr double distributionDivisor = 16.0;

void setError(ErrorDiffusionBufferError* destination,
              ErrorDiffusionBufferError error)
{
    if (destination != nullptr) {
        *destination = error;
    }
}

bool bypassOrderedDither(RgbSample input)
{
    return (input.red <= 8.0 && input.green <= 8.0 && input.blue <= 8.0)
        || (input.red >= 248.0 && input.green >= 248.0 && input.blue >= 248.0);
}

} // namespace

std::optional<DitherConfiguration> ditherConfiguration(DitherMode mode)
{
    switch (mode) {
    case DitherMode::None:
        return DitherConfiguration{};
    case DitherMode::FloydSteinberg:
        return DitherConfiguration{false, true, {3, 5, 1, 7, 0, 0}};
    case DitherMode::Atkinson:
        return DitherConfiguration{false, true, {2, 2, 2, 2, 1, 1}};
    case DitherMode::Pattern:
        return DitherConfiguration{false, true, {0, 8, 0, 8, 0, 0}};
    case DitherMode::Diagonal:
        return DitherConfiguration{false, true, {1, 3, 2, 3, 1, 1}};
    case DitherMode::Ordered:
        return DitherConfiguration{true, false, {}};
    case DitherMode::OrderedWithError:
        return DitherConfiguration{true, true, {1, 2, 2, 2, 0, 0}};
    }
    return std::nullopt;
}

std::optional<double> orderedDitherThreshold(OrderedDitherMapSize size,
                                             std::uint32_t x,
                                             std::uint32_t y,
                                             int brightness)
{
    if (brightness < 0 || brightness > 16) {
        return std::nullopt;
    }

    constexpr std::array<std::array<int, 2>, 2> map2{{
        {{0, 2}},
        {{3, 1}},
    }};
    constexpr std::array<std::array<int, 4>, 4> map4{{
        {{0, 8, 2, 10}},
        {{12, 4, 14, 6}},
        {{3, 11, 1, 9}},
        {{15, 7, 13, 5}},
    }};

    const double brightnessAdjustment = static_cast<double>(brightness) / 16.0;
    switch (size) {
    case OrderedDitherMapSize::TwoByTwo:
        return static_cast<double>(map2[x & 1U][y & 1U]) / 4.0
            - brightnessAdjustment;
    case OrderedDitherMapSize::FourByFour:
        return static_cast<double>(map4[x & 3U][y & 3U]) / 16.0
            - brightnessAdjustment;
    }
    return std::nullopt;
}

std::optional<RgbSample> applyOrderedDither(RgbSample input,
                                            std::uint32_t x,
                                            std::uint32_t y,
                                            OrderedDitherMapSize size,
                                            int brightness)
{
    const auto threshold = orderedDitherThreshold(size, x, y, brightness);
    if (!threshold) {
        return std::nullopt;
    }
    if (bypassOrderedDither(input)) {
        return input;
    }

    return RgbSample{
        input.red + input.red * *threshold,
        input.green + input.green * *threshold,
        input.blue + input.blue * *threshold,
    };
}

std::optional<ErrorDiffusionBuffer>
ErrorDiffusionBuffer::create(std::uint32_t width,
                             std::uint32_t height,
                             std::size_t maximumPixels,
                             ErrorDiffusionBufferError* error)
{
    if (width == 0 || height == 0) {
        setError(error, ErrorDiffusionBufferError::ZeroDimension);
        return std::nullopt;
    }
    if (static_cast<std::size_t>(width)
        > std::numeric_limits<std::size_t>::max() / height) {
        setError(error, ErrorDiffusionBufferError::SizeOverflow);
        return std::nullopt;
    }

    const std::size_t pixelCount = static_cast<std::size_t>(width) * height;
    if (pixelCount > maximumPixels) {
        setError(error, ErrorDiffusionBufferError::PixelLimitExceeded);
        return std::nullopt;
    }

    setError(error, ErrorDiffusionBufferError::None);
    return ErrorDiffusionBuffer(width, height, std::vector<RgbSample>(pixelCount));
}

ErrorDiffusionBuffer::ErrorDiffusionBuffer(std::uint32_t width,
                                           std::uint32_t height,
                                           std::vector<RgbSample> errors)
    : width_(width), height_(height), errors_(std::move(errors))
{
}

std::size_t ErrorDiffusionBuffer::index(std::uint32_t x, std::uint32_t y) const
{
    if (x >= width_ || y >= height_) {
        throw std::out_of_range("error-diffusion coordinate is outside the buffer");
    }
    return static_cast<std::size_t>(y) * width_ + x;
}

RgbSample ErrorDiffusionBuffer::errorAt(std::uint32_t x, std::uint32_t y) const
{
    return errors_[index(x, y)];
}

RgbSample ErrorDiffusionBuffer::adjustedSample(RgbSample source,
                                               std::uint32_t x,
                                               std::uint32_t y,
                                               ErrorAccumulationMode mode) const
{
    const RgbSample error = errorAt(x, y);
    const double divisor = mode == ErrorAccumulationMode::Average && y != 0
        ? 3.0
        : 1.0;
    return {
        source.red + error.red / divisor,
        source.green + error.green / divisor,
        source.blue + error.blue / divisor,
    };
}

void ErrorDiffusionBuffer::add(std::uint32_t x,
                               std::uint32_t y,
                               RgbSample error,
                               std::uint8_t weight)
{
    if (x >= width_ || y >= height_ || weight == 0) {
        return;
    }
    RgbSample& destination = errors_[static_cast<std::size_t>(y) * width_ + x];
    const double scale = static_cast<double>(weight) / distributionDivisor;
    destination.red += error.red * scale;
    destination.green += error.green * scale;
    destination.blue += error.blue * scale;
}

void ErrorDiffusionBuffer::distribute(std::uint32_t x,
                                      std::uint32_t y,
                                      RgbSample quantizationError,
                                      const ErrorDistributionKernel& kernel)
{
    (void)index(x, y);

    const bool hasNextRow = y < height_ - 1U;
    const bool hasSecondNextRow = y < height_ - 1U && y + 1U < height_ - 1U;
    const bool hasNextColumn = x < width_ - 1U;
    const bool hasSecondNextColumn = x < width_ - 1U && x + 1U < width_ - 1U;

    if (x > 0 && hasNextRow) {
        add(x - 1U, y + 1U, quantizationError, kernel.downLeft);
    }
    if (hasNextRow) {
        add(x, y + 1U, quantizationError, kernel.down);
    }
    if (hasNextColumn && hasNextRow) {
        add(x + 1U, y + 1U, quantizationError, kernel.downRight);
    }
    if (hasNextColumn) {
        add(x + 1U, y, quantizationError, kernel.right);
    }
    if (hasSecondNextColumn) {
        add(x + 2U, y, quantizationError, kernel.farRight);
    }
    if (hasSecondNextRow) {
        add(x, y + 2U, quantizationError, kernel.downTwo);
    }
}

} // namespace newconvert9918::core
