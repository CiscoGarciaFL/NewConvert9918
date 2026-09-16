#include "newconvert9918/core/ImageTransform.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <numbers>
#include <utility>
#include <vector>

namespace newconvert9918::core {
namespace {

struct AxisPlacement {
    std::uint32_t crop{};
    std::uint32_t destination{};
    std::uint32_t count{};
};

struct Contribution {
    std::size_t first{};
    std::vector<double> weights;
};

void setError(ImageTransformError* destination, ImageTransformError error)
{
    if (destination != nullptr) {
        *destination = error;
    }
}

bool isSupported(ScalingFilter filter)
{
    switch (filter) {
    case ScalingFilter::Box:
    case ScalingFilter::Gaussian:
    case ScalingFilter::Hamming:
    case ScalingFilter::Blackman:
    case ScalingFilter::Bilinear:
    case ScalingFilter::None:
        return true;
    }
    return false;
}

bool isSupported(ImageFillMode fillMode)
{
    switch (fillMode) {
    case ImageFillMode::Fit:
    case ImageFillMode::CropStart:
    case ImageFillMode::CropCenter:
    case ImageFillMode::CropEnd:
        return true;
    }
    return false;
}

double anchor(ImageFillMode fillMode)
{
    switch (fillMode) {
    case ImageFillMode::CropStart:
        return 0.0;
    case ImageFillMode::Fit:
    case ImageFillMode::CropCenter:
        return 0.5;
    case ImageFillMode::CropEnd:
        return 1.0;
    }
    return 0.5;
}

std::optional<std::uint32_t> roundedScale(std::uint32_t value,
                                          std::uint32_t numerator,
                                          std::uint32_t denominator)
{
    const long double scaled = static_cast<long double>(value)
        * static_cast<long double>(numerator) / static_cast<long double>(denominator);
    if (scaled > static_cast<long double>(std::numeric_limits<std::uint32_t>::max())) {
        return std::nullopt;
    }
    return std::max<std::uint32_t>(1, static_cast<std::uint32_t>(std::floor(scaled + 0.5L)));
}

AxisPlacement placeAxis(std::uint32_t scaled,
                        std::uint32_t target,
                        double anchorValue,
                        int offset)
{
    if (scaled >= target) {
        const std::uint32_t overflow = scaled - target;
        const auto base = static_cast<std::int64_t>(
            std::floor(static_cast<double>(overflow) * anchorValue));
        const auto adjusted = std::clamp<std::int64_t>(
            base + static_cast<std::int64_t>(offset), 0, overflow);
        return {
            .crop = static_cast<std::uint32_t>(adjusted),
            .destination = 0,
            .count = target,
        };
    }

    const std::uint32_t gap = target - scaled;
    return {
        .crop = 0,
        .destination = static_cast<std::uint32_t>(
            std::floor(static_cast<double>(gap) * anchorValue)),
        .count = scaled,
    };
}

double filterWidth(ScalingFilter filter)
{
    switch (filter) {
    case ScalingFilter::Box:
    case ScalingFilter::Hamming:
    case ScalingFilter::Blackman:
        return 0.5;
    case ScalingFilter::Bilinear:
        return 1.0;
    case ScalingFilter::Gaussian:
        return 3.0;
    case ScalingFilter::None:
        return 0.0;
    }
    return 0.0;
}

double filterWeight(ScalingFilter filter, double value)
{
    const double absolute = std::abs(value);
    switch (filter) {
    case ScalingFilter::Box:
        return absolute <= 0.5 ? 1.0 : 0.0;
    case ScalingFilter::Bilinear:
        return absolute < 1.0 ? 1.0 - absolute : 0.0;
    case ScalingFilter::Gaussian:
        return absolute <= 3.0
            ? std::exp(-(value * value) / 2.0) / std::sqrt(2.0 * std::numbers::pi)
            : 0.0;
    case ScalingFilter::Hamming:
        if (absolute > 0.5) {
            return 0.0;
        }
        return (0.54 + 0.46 * std::cos(2.0 * std::numbers::pi * value))
            * (value == 0.0 ? 1.0
                            : std::sin(std::numbers::pi * value)
                                / (std::numbers::pi * value));
    case ScalingFilter::Blackman:
        if (absolute > 0.5) {
            return 0.0;
        }
        return 0.42 + 0.5 * std::cos(std::numbers::pi * value)
            + 0.08 * std::cos(2.0 * std::numbers::pi * value);
    case ScalingFilter::None:
        return 0.0;
    }
    return 0.0;
}

std::vector<Contribution>
buildContributions(std::size_t sourceSize, std::size_t destinationSize, ScalingFilter filter)
{
    const double scale = static_cast<double>(destinationSize) / static_cast<double>(sourceSize);
    double width = filterWidth(filter);
    double filterScale = 1.0;
    if (scale < 1.0) {
        width /= scale;
        filterScale = scale;
    }

    std::vector<Contribution> contributions(destinationSize);
    for (std::size_t destination = 0; destination < destinationSize; ++destination) {
        const double center = static_cast<double>(destination) / scale;
        const std::size_t left = static_cast<std::size_t>(std::max<double>(
            0.0, std::floor(center - width)));
        const std::size_t right = static_cast<std::size_t>(std::min<double>(
            std::ceil(center + width), static_cast<double>(sourceSize - 1)));

        Contribution& contribution = contributions[destination];
        contribution.first = left;
        contribution.weights.reserve(right - left + 1);
        double total = 0.0;
        for (std::size_t source = left; source <= right; ++source) {
            const double weight = filterScale
                * filterWeight(filter,
                               filterScale * (center - static_cast<double>(source)));
            contribution.weights.push_back(weight);
            total += weight;
        }

        if (total > 0.0) {
            for (double& weight : contribution.weights) {
                weight /= total;
            }
        } else {
            const std::size_t nearest = std::min<std::size_t>(
                sourceSize - 1, static_cast<std::size_t>(std::floor(center + 0.5)));
            contribution.first = nearest;
            contribution.weights.assign(1, 1.0);
        }
    }
    return contributions;
}

std::uint8_t roundedChannel(double value)
{
    return static_cast<std::uint8_t>(std::clamp<long>(std::lround(value), 0, 255));
}

std::vector<std::uint8_t> resampleHorizontal(const std::uint8_t* source,
                                             std::size_t sourceWidth,
                                             std::size_t height,
                                             std::size_t sourceStride,
                                             std::size_t destinationWidth,
                                             std::size_t channelCount,
                                             ScalingFilter filter)
{
    const auto contributions = buildContributions(sourceWidth, destinationWidth, filter);
    const std::size_t destinationStride = destinationWidth * channelCount;
    std::vector<std::uint8_t> destination(destinationStride * height);
    for (std::size_t y = 0; y < height; ++y) {
        const std::uint8_t* sourceRow = source + y * sourceStride;
        std::uint8_t* destinationRow = destination.data() + y * destinationStride;
        for (std::size_t x = 0; x < destinationWidth; ++x) {
            const Contribution& contribution = contributions[x];
            for (std::size_t channel = 0; channel < channelCount; ++channel) {
                double value = 0.0;
                for (std::size_t sample = 0; sample < contribution.weights.size(); ++sample) {
                    const std::size_t sourceX = contribution.first + sample;
                    value += contribution.weights[sample]
                        * static_cast<double>(sourceRow[sourceX * channelCount + channel]);
                }
                destinationRow[x * channelCount + channel] = roundedChannel(value);
            }
        }
    }
    return destination;
}

std::vector<std::uint8_t> resampleVertical(const std::uint8_t* source,
                                           std::size_t width,
                                           std::size_t sourceHeight,
                                           std::size_t sourceStride,
                                           std::size_t destinationHeight,
                                           std::size_t channelCount,
                                           ScalingFilter filter)
{
    const auto contributions = buildContributions(sourceHeight, destinationHeight, filter);
    const std::size_t destinationStride = width * channelCount;
    std::vector<std::uint8_t> destination(destinationStride * destinationHeight);
    for (std::size_t y = 0; y < destinationHeight; ++y) {
        const Contribution& contribution = contributions[y];
        std::uint8_t* destinationRow = destination.data() + y * destinationStride;
        for (std::size_t x = 0; x < width; ++x) {
            for (std::size_t channel = 0; channel < channelCount; ++channel) {
                double value = 0.0;
                for (std::size_t sample = 0; sample < contribution.weights.size(); ++sample) {
                    const std::size_t sourceY = contribution.first + sample;
                    value += contribution.weights[sample]
                        * static_cast<double>(source[sourceY * sourceStride
                                                     + x * channelCount + channel]);
                }
                destinationRow[x * channelCount + channel] = roundedChannel(value);
            }
        }
    }
    return destination;
}

std::vector<std::uint8_t> tightlyPackedCopy(const RgbImage& source)
{
    const std::size_t rowBytes = source.minimumRowBytes();
    std::vector<std::uint8_t> result(rowBytes * source.height());
    for (std::uint32_t y = 0; y < source.height(); ++y) {
        const auto row = source.row(y);
        std::copy_n(row.begin(), rowBytes, result.begin() + y * rowBytes);
    }
    return result;
}

std::vector<std::uint8_t> resample(const RgbImage& source,
                                   std::uint32_t destinationWidth,
                                   std::uint32_t destinationHeight,
                                   ScalingFilter filter)
{
    const std::size_t channels = bytesPerPixel(source.pixelFormat());
    if (source.width() == destinationWidth && source.height() == destinationHeight) {
        return tightlyPackedCopy(source);
    }

    const std::size_t horizontalFirstPixels =
        static_cast<std::size_t>(destinationWidth) * source.height();
    const std::size_t verticalFirstPixels =
        static_cast<std::size_t>(source.width()) * destinationHeight;
    if (horizontalFirstPixels <= verticalFirstPixels) {
        auto horizontal = resampleHorizontal(source.bytes().data(),
                                             source.width(),
                                             source.height(),
                                             source.rowStride(),
                                             destinationWidth,
                                             channels,
                                             filter);
        return resampleVertical(horizontal.data(),
                                destinationWidth,
                                source.height(),
                                static_cast<std::size_t>(destinationWidth) * channels,
                                destinationHeight,
                                channels,
                                filter);
    }

    auto vertical = resampleVertical(source.bytes().data(),
                                     source.width(),
                                     source.height(),
                                     source.rowStride(),
                                     destinationHeight,
                                     channels,
                                     filter);
    return resampleHorizontal(vertical.data(),
                              source.width(),
                              destinationHeight,
                              static_cast<std::size_t>(source.width()) * channels,
                              destinationWidth,
                              channels,
                              filter);
}

void fillBackground(RgbImage& image, const ImageTransformOptions& options)
{
    const std::size_t channels = bytesPerPixel(image.pixelFormat());
    for (std::uint32_t y = 0; y < image.height(); ++y) {
        auto row = image.row(y);
        for (std::uint32_t x = 0; x < image.width(); ++x) {
            row[x * channels] = options.backgroundRed;
            row[x * channels + 1] = options.backgroundGreen;
            row[x * channels + 2] = options.backgroundBlue;
            if (channels == 4) {
                row[x * channels + 3] = options.backgroundAlpha;
            }
        }
    }
}

} // namespace

std::optional<ImageTransformPlan> planImageTransform(const RgbImage& source,
                                                     const ImageTransformOptions& options,
                                                     const ImageSizeLimits& limits,
                                                     ImageTransformError* error)
{
    if (options.targetWidth == 0 || options.targetHeight == 0) {
        setError(error, ImageTransformError::ZeroTargetDimension);
        return std::nullopt;
    }
    if (!isSupported(options.filter)) {
        setError(error, ImageTransformError::UnsupportedFilter);
        return std::nullopt;
    }
    if (!isSupported(options.fillMode)) {
        setError(error, ImageTransformError::UnsupportedFillMode);
        return std::nullopt;
    }

    std::uint32_t scaledWidth = source.width();
    std::uint32_t scaledHeight = source.height();
    if (options.filter != ScalingFilter::None) {
        const long double widthScale = static_cast<long double>(options.targetWidth)
            / static_cast<long double>(source.width());
        const long double heightScale = static_cast<long double>(options.targetHeight)
            / static_cast<long double>(source.height());
        const bool useWidth = options.fillMode == ImageFillMode::Fit
            ? widthScale <= heightScale
            : widthScale >= heightScale;
        const auto otherDimension = useWidth
            ? roundedScale(source.height(), options.targetWidth, source.width())
            : roundedScale(source.width(), options.targetHeight, source.height());
        if (!otherDimension.has_value()) {
            setError(error, ImageTransformError::ScaledImageLimitExceeded);
            return std::nullopt;
        }
        if (useWidth) {
            scaledWidth = options.targetWidth;
            scaledHeight = *otherDimension;
        } else {
            scaledWidth = *otherDimension;
            scaledHeight = options.targetHeight;
        }
    }

    const std::size_t channelCount = bytesPerPixel(source.pixelFormat());
    const ImageLayout scaledLayout{
        .width = scaledWidth,
        .height = scaledHeight,
        .pixelFormat = source.pixelFormat(),
        .rowStride = static_cast<std::size_t>(scaledWidth) * channelCount,
    };
    const ImageLayout outputLayout{
        .width = options.targetWidth,
        .height = options.targetHeight,
        .pixelFormat = source.pixelFormat(),
        .rowStride = static_cast<std::size_t>(options.targetWidth) * channelCount,
    };
    if (!validateImageLayout(scaledLayout, limits) || !validateImageLayout(outputLayout, limits)) {
        setError(error, ImageTransformError::ScaledImageLimitExceeded);
        return std::nullopt;
    }

    const double anchorValue = anchor(options.fillMode);
    const AxisPlacement horizontal = placeAxis(
        scaledWidth, options.targetWidth, anchorValue, options.horizontalOffset);
    const AxisPlacement vertical = placeAxis(
        scaledHeight, options.targetHeight, anchorValue, options.verticalOffset);
    setError(error, ImageTransformError::None);
    return ImageTransformPlan{
        .scaledWidth = scaledWidth,
        .scaledHeight = scaledHeight,
        .cropX = horizontal.crop,
        .cropY = vertical.crop,
        .destinationX = horizontal.destination,
        .destinationY = vertical.destination,
        .copyWidth = horizontal.count,
        .copyHeight = vertical.count,
    };
}

ImageTransformResult transformImage(const RgbImage& source,
                                    const ImageTransformOptions& options,
                                    const ImageSizeLimits& limits)
{
    ImageTransformResult result;
    const auto plan = planImageTransform(source, options, limits, &result.error);
    if (!plan.has_value()) {
        return result;
    }
    result.plan = *plan;

    const ScalingFilter resamplingFilter = options.filter == ScalingFilter::None
        ? ScalingFilter::Box
        : options.filter;
    std::vector<std::uint8_t> scaled = options.filter == ScalingFilter::None
        ? tightlyPackedCopy(source)
        : resample(source, plan->scaledWidth, plan->scaledHeight, resamplingFilter);

    ImageLayoutError imageError = ImageLayoutError::None;
    auto output = RgbImage::createTightlyPacked(options.targetWidth,
                                                options.targetHeight,
                                                source.pixelFormat(),
                                                limits,
                                                &imageError);
    if (!output.has_value()) {
        result.error = ImageTransformError::OutputImageRejected;
        return result;
    }
    fillBackground(*output, options);

    const std::size_t channels = bytesPerPixel(source.pixelFormat());
    const std::size_t scaledStride = static_cast<std::size_t>(plan->scaledWidth) * channels;
    const std::size_t copyBytes = static_cast<std::size_t>(plan->copyWidth) * channels;
    for (std::uint32_t y = 0; y < plan->copyHeight; ++y) {
        const std::uint8_t* sourceRow = scaled.data()
            + (static_cast<std::size_t>(plan->cropY + y) * scaledStride)
            + static_cast<std::size_t>(plan->cropX) * channels;
        auto destinationRow = output->row(plan->destinationY + y);
        std::memcpy(destinationRow.data()
                        + static_cast<std::size_t>(plan->destinationX) * channels,
                    sourceRow,
                    copyBytes);
    }

    result.error = ImageTransformError::None;
    result.image = std::move(output);
    return result;
}

} // namespace newconvert9918::core
