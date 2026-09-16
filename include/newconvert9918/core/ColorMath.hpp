#pragma once

#include "newconvert9918/core/ConversionSettings.hpp"
#include "newconvert9918/core/TargetData.hpp"

namespace newconvert9918::core {

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

} // namespace newconvert9918::core
