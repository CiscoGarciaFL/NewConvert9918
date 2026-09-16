#include "newconvert9918/core/PaletteSelection.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <utility>
#include <vector>

namespace newconvert9918::core {
namespace {

constexpr std::size_t channelCount = 3;
constexpr std::size_t rgb444ColorCount = 4096;

struct Point {
    std::array<std::uint8_t, channelCount> channel{};
};

struct Block {
    std::size_t begin{};
    std::size_t end{};
    std::array<std::uint8_t, channelCount> minimum{};
    std::array<std::uint8_t, channelCount> maximum{};
    std::size_t creationOrder{};

    [[nodiscard]] std::size_t size() const { return end - begin; }

    [[nodiscard]] std::size_t longestChannel() const
    {
        std::size_t selected = 0;
        int longest = static_cast<int>(maximum[0]) - minimum[0];
        for (std::size_t channelIndex = 1; channelIndex < channelCount; ++channelIndex) {
            const int length = static_cast<int>(maximum[channelIndex])
                - minimum[channelIndex];
            if (length > longest) {
                longest = length;
                selected = channelIndex;
            }
        }
        return selected;
    }

    [[nodiscard]] int longestLength() const
    {
        const std::size_t selected = longestChannel();
        return static_cast<int>(maximum[selected]) - minimum[selected];
    }
};

struct PopularColor {
    std::uint16_t value{};
    std::uint64_t count{};
};

bool validColorCount(std::size_t desiredColorCount)
{
    return desiredColorCount > 0 && desiredColorCount <= Palette::maximumColorCount;
}

bool supported(MedianCutColorDepth colorDepth)
{
    switch (colorDepth) {
    case MedianCutColorDepth::Rgb444:
    case MedianCutColorDepth::Rgb888:
        return true;
    }
    return false;
}

bool supported(PopularityWeighting weighting)
{
    switch (weighting) {
    case PopularityWeighting::Uniform:
    case PopularityWeighting::HorizontalCenter:
        return true;
    }
    return false;
}

Block makeBlock(const std::vector<Point>& points,
                std::size_t begin,
                std::size_t end,
                std::size_t creationOrder)
{
    Block block{
        .begin = begin,
        .end = end,
        .minimum = points[begin].channel,
        .maximum = points[begin].channel,
        .creationOrder = creationOrder,
    };
    for (std::size_t pointIndex = begin + 1; pointIndex < end; ++pointIndex) {
        for (std::size_t channelIndex = 0; channelIndex < channelCount; ++channelIndex) {
            block.minimum[channelIndex] = std::min(
                block.minimum[channelIndex], points[pointIndex].channel[channelIndex]);
            block.maximum[channelIndex] = std::max(
                block.maximum[channelIndex], points[pointIndex].channel[channelIndex]);
        }
    }
    return block;
}

std::vector<Point> collectPoints(const RgbImage& source, MedianCutColorDepth colorDepth)
{
    const std::size_t sourceChannels = bytesPerPixel(source.pixelFormat());
    std::vector<Point> points;
    points.reserve(static_cast<std::size_t>(source.width()) * source.height());
    for (std::uint32_t y = 0; y < source.height(); ++y) {
        const auto row = source.row(y);
        for (std::uint32_t x = 0; x < source.width(); ++x) {
            const std::uint8_t* pixel = row.data()
                + (static_cast<std::size_t>(x) * sourceChannels);
            Point point{{pixel[0], pixel[1], pixel[2]}};
            if (colorDepth == MedianCutColorDepth::Rgb444) {
                for (std::uint8_t& channel : point.channel) {
                    channel >>= 4;
                }
            }
            points.push_back(point);
        }
    }
    return points;
}

RgbColor averageColor(const std::vector<Point>& points,
                      const Block& block,
                      MedianCutColorDepth colorDepth)
{
    std::array<std::uint64_t, channelCount> sums{};
    for (std::size_t pointIndex = block.begin; pointIndex < block.end; ++pointIndex) {
        for (std::size_t channelIndex = 0; channelIndex < channelCount; ++channelIndex) {
            sums[channelIndex] += points[pointIndex].channel[channelIndex];
        }
    }

    std::array<std::uint8_t, channelCount> average{};
    for (std::size_t channelIndex = 0; channelIndex < channelCount; ++channelIndex) {
        average[channelIndex] = static_cast<std::uint8_t>(
            sums[channelIndex] / block.size());
        if (colorDepth == MedianCutColorDepth::Rgb444) {
            average[channelIndex] = static_cast<std::uint8_t>(
                (average[channelIndex] << 4) | average[channelIndex]);
        }
    }
    return {average[0], average[1], average[2]};
}

PaletteSelectionResult makeResult(std::vector<RgbColor> colors)
{
    PaletteSelectionResult result;
    PaletteError paletteError = PaletteError::None;
    result.palette = Palette::create(std::move(colors), &paletteError);
    if (!result.palette.has_value()) {
        result.error = PaletteSelectionError::PaletteRejected;
    }
    return result;
}

std::uint16_t rgb444(const std::uint8_t* pixel)
{
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(pixel[0] >> 4) << 8)
        | (static_cast<std::uint16_t>(pixel[1] >> 4) << 4)
        | static_cast<std::uint16_t>(pixel[2] >> 4));
}

RgbColor expandRgb444(std::uint16_t value)
{
    const auto expand = [](std::uint16_t nibble) {
        return static_cast<std::uint8_t>((nibble << 4) | nibble);
    };
    return {
        expand((value >> 8) & 0x0f),
        expand((value >> 4) & 0x0f),
        expand(value & 0x0f),
    };
}

std::uint64_t popularityWeight(std::uint32_t x,
                               std::uint32_t width,
                               PopularityWeighting weighting)
{
    if (weighting == PopularityWeighting::Uniform) {
        return 1;
    }
    const std::size_t section = std::min<std::size_t>(
        7, (static_cast<std::size_t>(x) * 8) / width);
    if (section == 0 || section == 7) {
        return 1;
    }
    if (section == 3 || section == 4) {
        return 3;
    }
    return 2;
}

std::vector<PopularColor> rankColors(
    const std::array<std::uint64_t, rgb444ColorCount>& counts)
{
    std::vector<PopularColor> ranking;
    for (std::size_t value = 0; value < counts.size(); ++value) {
        if (counts[value] != 0) {
            ranking.push_back({static_cast<std::uint16_t>(value), counts[value]});
        }
    }
    std::sort(ranking.begin(), ranking.end(), [](const PopularColor& left,
                                                  const PopularColor& right) {
        if (left.count != right.count) {
            return left.count > right.count;
        }
        return left.value < right.value;
    });
    return ranking;
}

bool neighboringRgb444(std::uint16_t left, std::uint16_t right)
{
    for (int shift : {8, 4, 0}) {
        const int leftChannel = (left >> shift) & 0x0f;
        const int rightChannel = (right >> shift) & 0x0f;
        if (std::abs(leftChannel - rightChannel) > 1) {
            return false;
        }
    }
    return true;
}

} // namespace

PaletteSelectionResult selectMedianCutPalette(const RgbImage& source,
                                              std::size_t desiredColorCount,
                                              MedianCutColorDepth colorDepth)
{
    PaletteSelectionResult result;
    if (!validColorCount(desiredColorCount)) {
        result.error = PaletteSelectionError::InvalidColorCount;
        return result;
    }
    if (!supported(colorDepth)) {
        result.error = PaletteSelectionError::UnsupportedColorDepth;
        return result;
    }

    std::vector<Point> points = collectPoints(source, colorDepth);
    std::vector<Block> blocks{makeBlock(points, 0, points.size(), 0)};
    std::size_t nextCreationOrder = 1;
    while (blocks.size() < desiredColorCount) {
        std::size_t selected = blocks.size();
        for (std::size_t index = 0; index < blocks.size(); ++index) {
            if (blocks[index].size() <= 1) {
                continue;
            }
            if (selected == blocks.size()
                || blocks[index].longestLength() > blocks[selected].longestLength()
                || (blocks[index].longestLength() == blocks[selected].longestLength()
                    && blocks[index].creationOrder < blocks[selected].creationOrder)) {
                selected = index;
            }
        }
        if (selected == blocks.size()) {
            break;
        }

        const Block block = blocks[selected];
        const std::size_t channel = block.longestChannel();
        std::sort(points.begin() + static_cast<std::ptrdiff_t>(block.begin),
                  points.begin() + static_cast<std::ptrdiff_t>(block.end),
                  [channel](const Point& left, const Point& right) {
                      if (left.channel[channel] != right.channel[channel]) {
                          return left.channel[channel] < right.channel[channel];
                      }
                      return left.channel < right.channel;
                  });
        const std::size_t middle = block.begin + ((block.size() + 1) / 2);
        blocks.erase(blocks.begin() + static_cast<std::ptrdiff_t>(selected));
        blocks.push_back(makeBlock(
            points, block.begin, middle, nextCreationOrder++));
        blocks.push_back(makeBlock(
            points, middle, block.end, nextCreationOrder++));
    }

    std::sort(blocks.begin(), blocks.end(), [](const Block& left, const Block& right) {
        if (left.longestLength() != right.longestLength()) {
            return left.longestLength() > right.longestLength();
        }
        return left.creationOrder < right.creationOrder;
    });
    std::vector<RgbColor> colors;
    colors.reserve(blocks.size());
    for (const Block& block : blocks) {
        colors.push_back(averageColor(points, block, colorDepth));
    }
    return makeResult(std::move(colors));
}

PaletteSelectionResult selectPopularPalette(const RgbImage& source,
                                            std::size_t desiredColorCount,
                                            PopularityWeighting weighting)
{
    PaletteSelectionResult result;
    if (!validColorCount(desiredColorCount)) {
        result.error = PaletteSelectionError::InvalidColorCount;
        return result;
    }
    if (!supported(weighting)) {
        result.error = PaletteSelectionError::UnsupportedWeighting;
        return result;
    }

    std::array<std::uint64_t, rgb444ColorCount> counts{};
    const std::size_t sourceChannels = bytesPerPixel(source.pixelFormat());
    for (std::uint32_t y = 0; y < source.height(); ++y) {
        const auto row = source.row(y);
        for (std::uint32_t x = 0; x < source.width(); ++x) {
            const std::uint8_t* pixel = row.data()
                + (static_cast<std::size_t>(x) * sourceChannels);
            counts[rgb444(pixel)] += popularityWeight(x, source.width(), weighting);
        }
    }

    for (;;) {
        const std::vector<PopularColor> ranking = rankColors(counts);
        const std::size_t considered = std::min(desiredColorCount, ranking.size());
        bool merged = false;
        for (std::size_t first = 0; first < considered; ++first) {
            for (std::size_t second = first + 1; second < considered; ++second) {
                if (counts[ranking[second].value] != 0
                    && neighboringRgb444(ranking[first].value, ranking[second].value)) {
                    counts[ranking[first].value] += counts[ranking[second].value];
                    counts[ranking[second].value] = 0;
                    merged = true;
                }
            }
        }
        if (!merged) {
            break;
        }
    }

    const std::vector<PopularColor> ranking = rankColors(counts);
    std::vector<RgbColor> colors;
    const std::size_t outputCount = std::min(desiredColorCount, ranking.size());
    colors.reserve(outputCount);
    for (std::size_t index = 0; index < outputCount; ++index) {
        colors.push_back(expandRgb444(ranking[index].value));
    }
    return makeResult(std::move(colors));
}

} // namespace newconvert9918::core
