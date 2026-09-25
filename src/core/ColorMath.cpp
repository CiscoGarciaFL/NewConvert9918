#include "newconvert9918/core/ColorMath.hpp"

namespace newconvert9918::core {

namespace {

double preparedYCrCbDistanceSquared(YCrCbSample left,
                                    YCrCbSample right,
                                    double lumaEmphasis)
{
    const double luminanceDifference =
        (left.luminance - right.luminance) * lumaEmphasis;
    const double redChromaDifference = left.redChroma - right.redChroma;
    const double blueChromaDifference = left.blueChroma - right.blueChroma;

    return (luminanceDifference * luminanceDifference)
        + (redChromaDifference * redChromaDifference)
        + (blueChromaDifference * blueChromaDifference);
}

} // namespace

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
    return preparedYCrCbDistanceSquared(
        toYCrCb(left), toYCrCb(right), lumaEmphasis);
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

ColorDistanceEvaluator::ColorDistanceEvaluator(const ConversionSettings& settings)
    : perceptual_(settings.perceptualColorMatching)
    , lumaEmphasis_(settings.lumaEmphasis)
    , weights_{
          .red = settings.perceptualRedWeight,
          .green = settings.perceptualGreenWeight,
          .blue = settings.perceptualBlueWeight,
      }
{
}

PreparedColorSample ColorDistanceEvaluator::prepare(RgbSample color) const
{
    return {
        .rgb = color,
        .yCrCb = toYCrCb(color),
    };
}

double ColorDistanceEvaluator::distanceSquared(
    RgbSample left,
    const PreparedColorSample& right) const
{
    if (perceptual_) {
        return perceptualRgbDistanceSquared(left, right.rgb, weights_);
    }
    return preparedYCrCbDistanceSquared(
        toYCrCb(left), right.yCrCb, lumaEmphasis_);
}

double ColorDistanceEvaluator::distanceSquared(
    const PreparedColorSample& left,
    RgbSample right) const
{
    if (perceptual_) {
        return perceptualRgbDistanceSquared(left.rgb, right, weights_);
    }
    return preparedYCrCbDistanceSquared(
        left.yCrCb, toYCrCb(right), lumaEmphasis_);
}

} // namespace newconvert9918::core
