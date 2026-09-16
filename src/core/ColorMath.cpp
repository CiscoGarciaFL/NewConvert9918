#include "newconvert9918/core/ColorMath.hpp"

namespace newconvert9918::core {

double luminance(RgbSample color)
{
    return (0.299 * color.red) + (0.587 * color.green) + (0.114 * color.blue);
}

YCrCbSample toYCrCb(RgbSample color)
{
    return {
        .luminance = luminance(color),
        .redChroma = (0.500 * color.red) - (0.419 * color.green)
            - (0.081 * color.blue),
        .blueChroma = (-0.169 * color.red) - (0.331 * color.green)
            + (0.500 * color.blue),
    };
}

double yCrCbDistanceSquared(RgbSample left, RgbSample right, double lumaEmphasis)
{
    const YCrCbSample leftYCrCb = toYCrCb(left);
    const YCrCbSample rightYCrCb = toYCrCb(right);
    const double luminanceDifference =
        (leftYCrCb.luminance - rightYCrCb.luminance) * lumaEmphasis;
    const double redChromaDifference = leftYCrCb.redChroma - rightYCrCb.redChroma;
    const double blueChromaDifference = leftYCrCb.blueChroma - rightYCrCb.blueChroma;

    return (luminanceDifference * luminanceDifference)
        + (redChromaDifference * redChromaDifference)
        + (blueChromaDifference * blueChromaDifference);
}

double perceptualRgbDistanceSquared(
    RgbSample left,
    RgbSample right,
    PerceptualRgbWeights weights)
{
    const double redDifference = left.red - right.red;
    const double greenDifference = left.green - right.green;
    const double blueDifference = left.blue - right.blue;

    return (redDifference * redDifference * weights.red)
        + (greenDifference * greenDifference * weights.green)
        + (blueDifference * blueDifference * weights.blue);
}

double colorDistanceSquared(
    RgbSample left,
    RgbSample right,
    const ConversionSettings& settings)
{
    if (settings.perceptualColorMatching) {
        return perceptualRgbDistanceSquared(
            left,
            right,
            {
                .red = settings.perceptualRedWeight,
                .green = settings.perceptualGreenWeight,
                .blue = settings.perceptualBlueWeight,
            });
    }

    return yCrCbDistanceSquared(left, right, settings.lumaEmphasis);
}

} // namespace newconvert9918::core
