#include "newconvert9918/core/ImageAdjustments.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace newconvert9918::core {
namespace {

constexpr std::size_t histogramBinCount = 256;

std::uint8_t brightness(const std::uint8_t* pixel)
{
    const unsigned int weighted = (299U * pixel[0]) + (587U * pixel[1])
        + (114U * pixel[2]);
    return static_cast<std::uint8_t>((weighted + 500U) / 1000U);
}

std::uint8_t addClamped(std::uint8_t channel, int difference)
{
    return static_cast<std::uint8_t>(
        std::clamp(static_cast<int>(channel) + difference, 0, 255));
}

void stretchBrightnessHistogram(RgbImage& image)
{
    std::array<std::size_t, histogramBinCount> histogram{};
    const std::size_t channels = bytesPerPixel(image.pixelFormat());
    for (std::uint32_t y = 0; y < image.height(); ++y) {
        const auto row = image.row(y);
        for (std::uint32_t x = 0; x < image.width(); ++x) {
            ++histogram[brightness(row.data() + (static_cast<std::size_t>(x) * channels))];
        }
    }

    std::size_t first = 0;
    while (first < histogram.size() && histogram[first] == 0) {
        ++first;
    }
    std::size_t last = histogram.size() - 1;
    while (last > first && histogram[last] == 0) {
        --last;
    }
    if (first >= last) {
        return;
    }

    const std::size_t pixelCount = static_cast<std::size_t>(image.width()) * image.height();
    const std::size_t firstCumulative = histogram[first];
    const std::size_t denominator = pixelCount - firstCumulative;
    const std::size_t outputRange = histogramOutputMaximum - histogramOutputMinimum;

    std::array<std::uint8_t, histogramBinCount> mapping{};
    std::size_t cumulative = 0;
    for (std::size_t value = 0; value < mapping.size(); ++value) {
        cumulative += histogram[value];
        if (value <= first) {
            mapping[value] = histogramOutputMinimum;
        } else {
            const std::size_t numerator = (cumulative - firstCumulative) * outputRange;
            mapping[value] = static_cast<std::uint8_t>(
                histogramOutputMinimum + ((numerator + (denominator / 2)) / denominator));
        }
    }

    for (std::uint32_t y = 0; y < image.height(); ++y) {
        auto row = image.row(y);
        for (std::uint32_t x = 0; x < image.width(); ++x) {
            std::uint8_t* pixel = row.data() + (static_cast<std::size_t>(x) * channels);
            const std::uint8_t originalBrightness = brightness(pixel);
            const int difference = static_cast<int>(mapping[originalBrightness])
                - static_cast<int>(originalBrightness);
            pixel[0] = addClamped(pixel[0], difference);
            pixel[1] = addClamped(pixel[1], difference);
            pixel[2] = addClamped(pixel[2], difference);
        }
    }
}

void applyGamma(RgbImage& image, double gamma)
{
    std::array<std::uint8_t, histogramBinCount> mapping{};
    const double exponent = 1.0 / gamma;
    for (std::size_t value = 0; value < mapping.size(); ++value) {
        const double normalized = static_cast<double>(value) / 255.0;
        const double corrected = std::pow(normalized, exponent) * 255.0;
        mapping[value] = static_cast<std::uint8_t>(
            std::clamp(static_cast<int>(corrected), 0, 255));
    }

    const std::size_t channels = bytesPerPixel(image.pixelFormat());
    for (std::uint32_t y = 0; y < image.height(); ++y) {
        auto row = image.row(y);
        for (std::uint32_t x = 0; x < image.width(); ++x) {
            std::uint8_t* pixel = row.data() + (static_cast<std::size_t>(x) * channels);
            pixel[0] = mapping[pixel[0]];
            pixel[1] = mapping[pixel[1]];
            pixel[2] = mapping[pixel[2]];
        }
    }
}

} // namespace

ImageAdjustmentResult adjustImage(const RgbImage& source,
                                  const ConversionSettings& settings,
                                  const ImageSizeLimits& limits)
{
    ImageAdjustmentResult result;
    if (!std::isfinite(settings.gamma) || settings.gamma <= 0.0) {
        result.error = ImageAdjustmentError::InvalidGamma;
        return result;
    }
    if (!validateImageLayout(source.layout(), limits)) {
        result.error = ImageAdjustmentError::OutputImageRejected;
        return result;
    }

    auto output = RgbImage::create(source.layout(), source.bytes(), limits);
    if (!output.has_value()) {
        result.error = ImageAdjustmentError::OutputImageRejected;
        return result;
    }

    if (settings.stretchHistogram) {
        stretchBrightnessHistogram(*output);
    }
    if (settings.gamma != 1.0) {
        applyGamma(*output, settings.gamma);
    }

    result.image = std::move(output);
    return result;
}

} // namespace newconvert9918::core
