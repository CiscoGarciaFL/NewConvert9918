#include "retrovdp/core/YamahaVdpConverter.hpp"

#include "retrovdp/core/ColorMath.hpp"
#include "retrovdp/core/PaletteSelection.hpp"
#include "retrovdp/core/TargetProfile.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace retrovdp::core {
namespace {

ConversionResult failure(std::string code, std::string message)
{
    return {
        .status = ConversionStatus::Failed,
        .preview = std::nullopt,
        .diagnostics = {{DiagnosticSeverity::Error, std::move(code), std::move(message)}},
        .target = std::nullopt,
    };
}

ConversionResult cancelled()
{
    return {.status = ConversionStatus::Cancelled};
}

RgbColor pixel(const RgbImage& image, std::uint32_t x, std::uint32_t y)
{
    const auto row = image.row(y);
    const std::size_t offset = static_cast<std::size_t>(x)
        * bytesPerPixel(image.pixelFormat());
    return {row[offset], row[offset + 1U], row[offset + 2U]};
}

void setPixel(RgbImage& image, std::uint32_t x, std::uint32_t y, RgbColor color)
{
    auto row = image.row(y);
    const std::size_t offset = static_cast<std::size_t>(x)
        * bytesPerPixel(image.pixelFormat());
    row[offset] = color.red;
    row[offset + 1U] = color.green;
    row[offset + 2U] = color.blue;
    if (image.pixelFormat() == PixelFormat::Rgba8888) row[offset + 3U] = 255U;
}

std::uint8_t to3(std::uint8_t value)
{
    return static_cast<std::uint8_t>((static_cast<unsigned>(value) * 7U + 127U) / 255U);
}

std::uint8_t from3(std::uint8_t value)
{
    return static_cast<std::uint8_t>((static_cast<unsigned>(value) * 255U + 3U) / 7U);
}

std::uint8_t to2(std::uint8_t value)
{
    return static_cast<std::uint8_t>((static_cast<unsigned>(value) * 3U + 127U) / 255U);
}

std::uint8_t from2(std::uint8_t value)
{
    return static_cast<std::uint8_t>(value * 85U);
}

std::optional<Palette> selectYamahaPalette(const RgbImage& source,
                                           std::size_t colorCount,
                                           PaletteSelectionMode selection)
{
    auto selected = selection == PaletteSelectionMode::Popularity
        ? selectPopularPalette(source, colorCount, PopularityWeighting::Uniform)
        : selectMedianCutPalette(source, colorCount, MedianCutColorDepth::Rgb888);
    if (!selected || !selected.palette) return std::nullopt;

    std::vector<RgbColor> colors;
    colors.reserve(colorCount);
    for (const auto color : selected.palette->colors()) {
        colors.push_back({from3(to3(color.red)), from3(to3(color.green)),
                          from3(to3(color.blue))});
    }
    while (colors.size() < colorCount) colors.push_back({});
    return Palette::create(std::move(colors));
}

std::vector<std::uint8_t> encodePalette(const Palette& palette)
{
    std::vector<std::uint8_t> bytes;
    bytes.reserve(palette.size() * 2U);
    for (const auto color : palette.colors()) {
        bytes.push_back(static_cast<std::uint8_t>((to3(color.red) << 4U)
                                                  | to3(color.blue)));
        bytes.push_back(to3(color.green));
    }
    return bytes;
}

std::uint8_t nearestPaletteIndex(RgbColor source,
                                 const Palette& palette,
                                 const ColorDistanceEvaluator& distance)
{
    double bestDistance = std::numeric_limits<double>::max();
    std::uint8_t best = 0;
    for (std::size_t index = 0; index < palette.size(); ++index) {
        const double candidate = distance.distanceSquared(
            RgbSample(source), distance.prepare(RgbSample(palette.at(index))));
        if (candidate < bestDistance) {
            bestDistance = candidate;
            best = static_cast<std::uint8_t>(index);
        }
    }
    return best;
}

RgbColor decodeGrb332(std::uint8_t value)
{
    return {from3(static_cast<std::uint8_t>((value >> 2U) & 7U)),
            from3(static_cast<std::uint8_t>((value >> 5U) & 7U)),
            from2(static_cast<std::uint8_t>(value & 3U))};
}

std::uint8_t encodeGrb332(RgbColor color)
{
    return static_cast<std::uint8_t>((to3(color.green) << 5U)
                                     | (to3(color.red) << 2U)
                                     | to2(color.blue));
}

int sign6(unsigned value)
{
    return (value & 0x20U) != 0U ? static_cast<int>(value) - 64
                                 : static_cast<int>(value);
}

RgbColor decodeYjk(std::uint8_t y, int j, int k)
{
    const auto channel = [](int value) {
        return static_cast<std::uint8_t>(std::clamp(value, 0, 31) * 255 / 31);
    };
    return {
        channel(static_cast<int>(y) + j),
        channel(static_cast<int>(y) + k),
        channel((5 * static_cast<int>(y) - 2 * j - k) / 4),
    };
}

struct YjkGroup {
    std::array<std::uint8_t, 4> bytes{};
    std::array<RgbColor, 4> colors{};
};

YjkGroup encodeYjkGroup(const std::array<RgbColor, 4>& source,
                        bool attributes,
                        const Palette* palette,
                        const ConversionSettings& settings)
{
    std::array<int, 4> sourceR{};
    std::array<int, 4> sourceG{};
    std::array<int, 4> sourceB{};
    int jSum = 0;
    int kSum = 0;
    for (std::size_t index = 0; index < source.size(); ++index) {
        sourceR[index] = (static_cast<int>(source[index].red) * 31 + 127) / 255;
        sourceG[index] = (static_cast<int>(source[index].green) * 31 + 127) / 255;
        sourceB[index] = (static_cast<int>(source[index].blue) * 31 + 127) / 255;
        const int y = std::clamp((sourceB[index] * 4 + sourceR[index] * 2
                                  + sourceG[index] + 3) / 7,
                                 0, 31);
        jSum += sourceR[index] - y;
        kSum += sourceG[index] - y;
    }
    const int j = std::clamp(static_cast<int>(std::lround(jSum / 4.0)), -32, 31);
    const int k = std::clamp(static_cast<int>(std::lround(kSum / 4.0)), -32, 31);
    const unsigned encodedJ = static_cast<unsigned>(j) & 0x3fU;
    const unsigned encodedK = static_cast<unsigned>(k) & 0x3fU;
    constexpr std::array<unsigned, 4> chromaShift{0, 3, 0, 3};

    YjkGroup result;
    for (std::size_t index = 0; index < source.size(); ++index) {
        int bestY = 0;
        double bestDistance = std::numeric_limits<double>::max();
        for (int y = 0; y < 32; ++y) {
            if (attributes && (y & 1) != 0) continue;
            const auto color = decodeYjk(static_cast<std::uint8_t>(y), j, k);
            const double candidate = colorDistanceSquared(
                RgbSample(source[index]), RgbSample(color), settings);
            if (candidate < bestDistance) {
                bestDistance = candidate;
                bestY = y;
            }
        }

        const unsigned chroma = index < 2 ? encodedK : encodedJ;
        result.bytes[index] = static_cast<std::uint8_t>(
            (static_cast<unsigned>(bestY) << 3U)
            | ((chroma >> chromaShift[index]) & 7U));
        result.colors[index] = decodeYjk(static_cast<std::uint8_t>(bestY), j, k);

        if (attributes && palette != nullptr) {
            const ColorDistanceEvaluator distance(settings);
            const std::uint8_t paletteIndex = nearestPaletteIndex(
                source[index], *palette, distance);
            const double paletteDistance = colorDistanceSquared(
                RgbSample(source[index]), RgbSample(palette->at(paletteIndex)), settings);
            if (paletteDistance < bestDistance) {
                result.bytes[index] = static_cast<std::uint8_t>(
                    (paletteIndex << 4U) | 0x08U
                    | ((chroma >> chromaShift[index]) & 7U));
                result.colors[index] = palette->at(paletteIndex);
            }
        }
    }
    return result;
}

ConversionResult convertPaletted(const RgbImage& source,
                                 const ConversionSettings& settings,
                                 std::size_t colorCount,
                                 unsigned pixelsPerByte,
                                 CancellationToken cancellation,
                                 ConversionProgressCallback progress)
{
    auto palette = selectYamahaPalette(source, colorCount, settings.paletteSelection);
    if (!palette) {
        return failure("yamaha-palette-selection-failed",
                       "The Yamaha programmable palette could not be selected.");
    }
    auto preview = RgbImage::createTightlyPacked(
        source.width(), source.height(), source.pixelFormat());
    if (!preview) {
        return failure("yamaha-preview-allocation-failed",
                       "The Yamaha bitmap preview could not be allocated.");
    }

    std::vector<std::uint8_t> framebuffer;
    framebuffer.reserve(static_cast<std::size_t>(source.width()) * source.height()
                        / pixelsPerByte);
    const ColorDistanceEvaluator distance(settings);
    for (std::uint32_t y = 0; y < source.height(); ++y) {
        if (cancellation.isCancellationRequested()) return cancelled();
        for (std::uint32_t x = 0; x < source.width(); x += pixelsPerByte) {
            std::uint8_t packed = 0;
            for (unsigned part = 0; part < pixelsPerByte; ++part) {
                const std::uint8_t index = nearestPaletteIndex(
                    pixel(source, x + part, y), *palette, distance);
                const unsigned bits = 8U / pixelsPerByte;
                packed = static_cast<std::uint8_t>(packed
                    | (index << (8U - bits * (part + 1U))));
                setPixel(*preview, x + part, y, palette->at(index));
            }
            framebuffer.push_back(packed);
        }
        if (progress
            && ((y + 1U) % 8U == 0U || y + 1U == source.height())) {
            progress(*preview, y + 1U, source.height());
        }
    }

    TargetMemoryImage target{
        .profile = settings.targetProfile,
        .mode = settings.mode,
        .palette = palette,
        .tables = {
            {TargetTableRole::Framebuffer, std::move(framebuffer)},
            {TargetTableRole::Palette, encodePalette(*palette)},
        },
    };
    return {.status = ConversionStatus::Succeeded,
            .preview = std::move(preview),
            .target = std::move(target)};
}

ConversionResult convertDirect(const RgbImage& source,
                               const ConversionSettings& settings,
                               CancellationToken cancellation,
                               ConversionProgressCallback progress)
{
    auto preview = RgbImage::createTightlyPacked(
        source.width(), source.height(), source.pixelFormat());
    if (!preview) {
        return failure("yamaha-preview-allocation-failed",
                       "The Yamaha bitmap preview could not be allocated.");
    }
    std::vector<std::uint8_t> framebuffer;
    framebuffer.reserve(static_cast<std::size_t>(source.width()) * source.height());
    for (std::uint32_t y = 0; y < source.height(); ++y) {
        if (cancellation.isCancellationRequested()) return cancelled();
        for (std::uint32_t x = 0; x < source.width(); ++x) {
            const std::uint8_t encoded = encodeGrb332(pixel(source, x, y));
            framebuffer.push_back(encoded);
            setPixel(*preview, x, y, decodeGrb332(encoded));
        }
        if (progress
            && ((y + 1U) % 8U == 0U || y + 1U == source.height())) {
            progress(*preview, y + 1U, source.height());
        }
    }
    TargetMemoryImage target{
        .profile = settings.targetProfile,
        .mode = settings.mode,
        .tables = {{TargetTableRole::Framebuffer, std::move(framebuffer)}},
    };
    return {.status = ConversionStatus::Succeeded,
            .preview = std::move(preview),
            .target = std::move(target)};
}

ConversionResult convertYjk(const RgbImage& source,
                            const ConversionSettings& settings,
                            bool attributes,
                            CancellationToken cancellation,
                            ConversionProgressCallback progress)
{
    std::optional<Palette> palette;
    if (attributes) {
        palette = selectYamahaPalette(source, 16, settings.paletteSelection);
        if (!palette) {
            return failure("yamaha-palette-selection-failed",
                           "The V9958 YAE palette could not be selected.");
        }
    }
    auto preview = RgbImage::createTightlyPacked(
        source.width(), source.height(), source.pixelFormat());
    if (!preview) {
        return failure("yamaha-preview-allocation-failed",
                       "The V9958 YJK preview could not be allocated.");
    }
    std::vector<std::uint8_t> framebuffer;
    framebuffer.reserve(static_cast<std::size_t>(source.width()) * source.height());
    for (std::uint32_t y = 0; y < source.height(); ++y) {
        if (cancellation.isCancellationRequested()) return cancelled();
        for (std::uint32_t x = 0; x < source.width(); x += 4U) {
            std::array<RgbColor, 4> colors{};
            for (std::size_t part = 0; part < colors.size(); ++part)
                colors[part] = pixel(source, x + static_cast<std::uint32_t>(part), y);
            const auto encoded = encodeYjkGroup(colors, attributes,
                                                palette ? &*palette : nullptr,
                                                settings);
            framebuffer.insert(framebuffer.end(), encoded.bytes.begin(), encoded.bytes.end());
            for (std::size_t part = 0; part < encoded.colors.size(); ++part)
                setPixel(*preview, x + static_cast<std::uint32_t>(part), y,
                         encoded.colors[part]);
        }
        if (progress
            && ((y + 1U) % 8U == 0U || y + 1U == source.height())) {
            progress(*preview, y + 1U, source.height());
        }
    }
    std::vector<TargetMemoryTable> tables{
        {TargetTableRole::Framebuffer, std::move(framebuffer)},
    };
    if (palette) tables.push_back({TargetTableRole::Palette, encodePalette(*palette)});
    TargetMemoryImage target{
        .profile = settings.targetProfile,
        .mode = settings.mode,
        .palette = palette,
        .tables = std::move(tables),
    };
    return {.status = ConversionStatus::Succeeded,
            .preview = std::move(preview),
            .target = std::move(target)};
}

} // namespace

ConversionResult convertYamahaBitmap(const RgbImage& source,
                                     const ConversionSettings& settings,
                                     CancellationToken cancellation,
                                     ConversionProgressCallback progress)
{
    const auto& descriptor = displayMode(settings.mode);
    if (source.width() != descriptor.geometry.width
        || source.height() != descriptor.geometry.height) {
        return failure("yamaha-source-size",
                       "The source geometry does not match the Yamaha display mode.");
    }
    switch (settings.mode) {
    case ConversionMode::Screen5V9938:
        return convertPaletted(source, settings, 16, 2, cancellation, std::move(progress));
    case ConversionMode::Screen6V9938:
        return convertPaletted(source, settings, 4, 4, cancellation, std::move(progress));
    case ConversionMode::Screen7V9938:
        return convertPaletted(source, settings, 16, 2, cancellation, std::move(progress));
    case ConversionMode::Screen8V9938:
        return convertDirect(source, settings, cancellation, std::move(progress));
    case ConversionMode::Screen10V9958:
    case ConversionMode::Screen11V9958:
        return convertYjk(source, settings, true, cancellation, std::move(progress));
    case ConversionMode::Screen12V9958:
        return convertYjk(source, settings, false, cancellation, std::move(progress));
    default:
        return failure("yamaha-unsupported-mode",
                       "The requested mode is not a native Yamaha bitmap mode.");
    }
}

} // namespace retrovdp::core
