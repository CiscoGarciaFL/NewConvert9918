#pragma once

#include "retrovdp/core/ConversionSettings.hpp"
#include "retrovdp/core/TargetData.hpp"

namespace retrovdp::core {

// Color matching works on doubles because error diffusion can temporarily move
// channels outside the stored 0-255 range.
struct RgbSample {
    double red{};
    double green{};
    double blue{};

    constexpr RgbSample() = default;
    constexpr RgbSample(double redValue, double greenValue, double blueValue)
        : red(redValue), green(greenValue), blue(blueValue)
    {
    }
    constexpr explicit RgbSample(RgbColor color)
        : red(color.red), green(color.green), blue(color.blue)
    {
    }
};

struct YCrCbSample {
    double luminance{};
    double redChroma{};
    double blueChroma{};
};

struct PerceptualRgbWeights {
    double red{0.30};
    double green{0.52};
    double blue{0.18};
};

// Precomputes the fixed side of a color comparison. The converters compare
// millions of changing source samples against a very small set of palette or
// mixed colors, so keeping the fixed YCrCb transform avoids repeating the same
// work in their innermost loops.
struct PreparedColorSample {
    RgbSample rgb;
    YCrCbSample yCrCb;
};

class ColorDistanceEvaluator final {
public:
    explicit ColorDistanceEvaluator(const ConversionSettings& settings);

    [[nodiscard]] PreparedColorSample prepare(RgbSample color) const;
    [[nodiscard]] double distanceSquared(
        RgbSample left,
        const PreparedColorSample& right) const;
    [[nodiscard]] double distanceSquared(
        const PreparedColorSample& left,
        RgbSample right) const;

private:
    bool perceptual_{};
    double lumaEmphasis_{1.2};
    PerceptualRgbWeights weights_{};
};

// Hot-loop variant used after the fixed color has already been prepared. The
// legacy converter compiled separate perceptual and YCrCb quantizers; the
// template parameter gives modern compilers the same opportunity to inline
// the selected formula without a per-pixel mode branch or out-of-line call.
template<bool Perceptual>
[[nodiscard]] inline double preparedColorDistanceSquared(
    RgbSample left,
    const PreparedColorSample& right,
    const ConversionSettings& settings)
{
    if constexpr (Perceptual) {
        const double redDifference = left.red - right.rgb.red;
        const double greenDifference = left.green - right.rgb.green;
        const double blueDifference = left.blue - right.rgb.blue;
        return (redDifference * redDifference * settings.perceptualRedWeight)
            + (greenDifference * greenDifference * settings.perceptualGreenWeight)
            + (blueDifference * blueDifference * settings.perceptualBlueWeight);
    }

    const double luminanceValue =
        (0.299 * left.red) + (0.587 * left.green) + (0.114 * left.blue);
    const double redChroma =
        (0.500 * left.red) - (0.419 * left.green) - (0.081 * left.blue);
    const double blueChroma =
        (-0.169 * left.red) - (0.331 * left.green) + (0.500 * left.blue);
    const double luminanceDifference =
        (luminanceValue - right.yCrCb.luminance) * settings.lumaEmphasis;
    const double redChromaDifference = redChroma - right.yCrCb.redChroma;
    const double blueChromaDifference = blueChroma - right.yCrCb.blueChroma;
    return (luminanceDifference * luminanceDifference)
        + (redChromaDifference * redChromaDifference)
        + (blueChromaDifference * blueChromaDifference);
}

template<bool Perceptual>
[[nodiscard]] inline double preparedColorDistanceSquared(
    const PreparedColorSample& left,
    RgbSample right,
    const ConversionSettings& settings)
{
    if constexpr (Perceptual) {
        const double redDifference = left.rgb.red - right.red;
        const double greenDifference = left.rgb.green - right.green;
        const double blueDifference = left.rgb.blue - right.blue;
        return (redDifference * redDifference * settings.perceptualRedWeight)
            + (greenDifference * greenDifference * settings.perceptualGreenWeight)
            + (blueDifference * blueDifference * settings.perceptualBlueWeight);
    }

    const double luminanceValue =
        (0.299 * right.red) + (0.587 * right.green) + (0.114 * right.blue);
    const double redChroma =
        (0.500 * right.red) - (0.419 * right.green) - (0.081 * right.blue);
    const double blueChroma =
        (-0.169 * right.red) - (0.331 * right.green) + (0.500 * right.blue);
    const double luminanceDifference =
        (left.yCrCb.luminance - luminanceValue) * settings.lumaEmphasis;
    const double redChromaDifference = left.yCrCb.redChroma - redChroma;
    const double blueChromaDifference = left.yCrCb.blueChroma - blueChroma;
    return (luminanceDifference * luminanceDifference)
        + (redChromaDifference * redChromaDifference)
        + (blueChromaDifference * blueChromaDifference);
}

[[nodiscard]] double luminance(RgbSample color);

// This compatibility transform intentionally omits the digital-video offsets.
[[nodiscard]] YCrCbSample toYCrCb(RgbSample color);

[[nodiscard]] double yCrCbDistanceSquared(
    RgbSample left,
    RgbSample right,
    double lumaEmphasis = 1.2);

[[nodiscard]] double perceptualRgbDistanceSquared(
    RgbSample left,
    RgbSample right,
    PerceptualRgbWeights weights = {});

// Selects the legacy YCrCb or perceptual RGB metric from conversion settings.
[[nodiscard]] double colorDistanceSquared(
    RgbSample left,
    RgbSample right,
    const ConversionSettings& settings);

} // namespace retrovdp::core
