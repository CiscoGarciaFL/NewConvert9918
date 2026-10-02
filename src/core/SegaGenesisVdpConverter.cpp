#include "retrovdp/core/SegaGenesisVdpConverter.hpp"

#include "retrovdp/core/ColorMath.hpp"
#include "retrovdp/core/Dithering.hpp"
#include "retrovdp/core/TargetProfile.hpp"
#include "retrovdp/core/Validation.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace retrovdp::core {
namespace {

constexpr std::size_t tileSize = 8;
constexpr std::size_t tilePixelCount = tileSize * tileSize;
constexpr std::size_t hardwareColorCount = 512;
constexpr std::size_t paletteCount = 4;
constexpr std::size_t colorsPerPalette = 16;
constexpr std::size_t opaqueColorsPerPalette = 15;
constexpr std::size_t registerCount = 24;

using ColorHistogram = std::array<std::uint32_t, hardwareColorCount>;
using HardwarePalette = std::array<std::uint16_t, colorsPerPalette>;
using IndexedTile = std::array<std::uint8_t, tilePixelCount>;
using HardwareDistances = std::vector<double>;

struct Tile {
    IndexedTile pixels{};
    std::uint8_t paletteBank{};
};

struct PatternReference {
    std::size_t index{};
    bool horizontalFlip{};
    bool verticalFlip{};
};

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

bool isGenesisMode(ConversionMode mode)
{
    return mode == ConversionMode::Mode5GenesisH32
        || mode == ConversionMode::Mode5GenesisH40
        || mode == ConversionMode::Mode5GenesisH32Pal
        || mode == ConversionMode::Mode5GenesisH40Pal;
}

bool isH40(ConversionMode mode)
{
    return mode == ConversionMode::Mode5GenesisH40
        || mode == ConversionMode::Mode5GenesisH40Pal;
}

bool isPal30(ConversionMode mode)
{
    return mode == ConversionMode::Mode5GenesisH32Pal
        || mode == ConversionMode::Mode5GenesisH40Pal;
}

std::uint8_t to3(std::uint8_t value)
{
    return static_cast<std::uint8_t>(
        (static_cast<unsigned>(value) * 7U + 127U) / 255U);
}

std::uint16_t hardwareCode(RgbColor color)
{
    return static_cast<std::uint16_t>(to3(color.red)
        | (to3(color.green) << 3U) | (to3(color.blue) << 6U));
}

RgbColor hardwareColor(std::uint16_t code)
{
    return {
        static_cast<std::uint8_t>((code & 0x07U) * 255U / 7U),
        static_cast<std::uint8_t>(((code >> 3U) & 0x07U) * 255U / 7U),
        static_cast<std::uint8_t>(((code >> 6U) & 0x07U) * 255U / 7U),
    };
}

RgbColor sourcePixel(const RgbImage& image, std::uint32_t x, std::uint32_t y)
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

double distanceAt(const HardwareDistances& distances,
                  std::size_t left, std::size_t right)
{
    return distances[left * hardwareColorCount + right];
}

HardwareDistances makeHardwareDistances(const ConversionSettings& settings)
{
    HardwareDistances result(hardwareColorCount * hardwareColorCount);
    for (std::size_t left = 0; left < hardwareColorCount; ++left) {
        for (std::size_t right = 0; right < hardwareColorCount; ++right) {
            result[left * hardwareColorCount + right] = colorDistanceSquared(
                RgbSample(hardwareColor(static_cast<std::uint16_t>(left))),
                RgbSample(hardwareColor(static_cast<std::uint16_t>(right))), settings);
        }
    }
    return result;
}

std::uint16_t mostPopularColor(const ColorHistogram& histogram)
{
    return static_cast<std::uint16_t>(std::max_element(
        histogram.begin(), histogram.end()) - histogram.begin());
}

HardwarePalette selectPalette(const ColorHistogram& histogram,
                              std::uint16_t background,
                              PaletteSelectionMode selection,
                              const HardwareDistances& distances)
{
    HardwarePalette result{};
    result[0] = background;
    std::array<bool, hardwareColorCount> selected{};
    selected[background] = true;

    if (selection == PaletteSelectionMode::Popularity) {
        for (std::size_t entry = 1; entry <= opaqueColorsPerPalette; ++entry) {
            std::size_t best = 0;
            for (std::size_t color = 0; color < hardwareColorCount; ++color) {
                if (!selected[color]
                    && (selected[best] || histogram[color] > histogram[best])) {
                    best = color;
                }
            }
            result[entry] = static_cast<std::uint16_t>(best);
            selected[best] = true;
        }
        return result;
    }

    std::array<double, hardwareColorCount> nearest{};
    for (std::size_t color = 0; color < hardwareColorCount; ++color)
        nearest[color] = distanceAt(distances, color, background);
    for (std::size_t entry = 1; entry <= opaqueColorsPerPalette; ++entry) {
        std::size_t best = 0;
        long double bestGain = -1.0;
        for (std::size_t candidate = 0; candidate < hardwareColorCount; ++candidate) {
            if (selected[candidate]) continue;
            long double gain = 0.0;
            for (std::size_t color = 0; color < hardwareColorCount; ++color) {
                if (histogram[color] == 0) continue;
                gain += static_cast<long double>(histogram[color])
                    * std::max(0.0, nearest[color]
                        - distanceAt(distances, color, candidate));
            }
            if (gain > bestGain) {
                bestGain = gain;
                best = candidate;
            }
        }
        result[entry] = static_cast<std::uint16_t>(best);
        selected[best] = true;
        for (std::size_t color = 0; color < hardwareColorCount; ++color) {
            nearest[color] = std::min(
                nearest[color], distanceAt(distances, color, best));
        }
    }
    return result;
}

double paletteError(const ColorHistogram& histogram,
                    const HardwarePalette& palette,
                    const HardwareDistances& distances)
{
    double result = 0.0;
    for (std::size_t color = 0; color < hardwareColorCount; ++color) {
        if (histogram[color] == 0) continue;
        double nearest = std::numeric_limits<double>::max();
        for (const std::uint16_t entry : palette)
            nearest = std::min(nearest, distanceAt(distances, color, entry));
        result += nearest * histogram[color];
    }
    return result;
}

std::array<HardwarePalette, paletteCount>
selectPaletteBanks(const std::vector<ColorHistogram>& tileHistograms,
                   PaletteSelectionMode selection,
                   const HardwareDistances& distances,
                   std::vector<std::uint8_t>& assignments)
{
    ColorHistogram all{};
    for (const auto& tile : tileHistograms) {
        for (std::size_t color = 0; color < hardwareColorCount; ++color)
            all[color] += tile[color];
    }
    const std::uint16_t background = mostPopularColor(all);

    std::vector<std::size_t> ordered(tileHistograms.size());
    std::iota(ordered.begin(), ordered.end(), 0U);
    std::ranges::sort(ordered, [&tileHistograms](std::size_t left, std::size_t right) {
        return mostPopularColor(tileHistograms[left])
            < mostPopularColor(tileHistograms[right]);
    });

    std::array<HardwarePalette, paletteCount> palettes{};
    assignments.assign(tileHistograms.size(), 0);
    for (std::size_t rank = 0; rank < ordered.size(); ++rank) {
        assignments[ordered[rank]] = static_cast<std::uint8_t>(
            std::min(paletteCount - 1, rank * paletteCount / ordered.size()));
    }

    for (int iteration = 0; iteration < 6; ++iteration) {
        std::array<ColorHistogram, paletteCount> bankHistograms{};
        std::array<std::size_t, paletteCount> bankTileCounts{};
        for (std::size_t tile = 0; tile < tileHistograms.size(); ++tile) {
            const std::size_t bank = assignments[tile];
            ++bankTileCounts[bank];
            for (std::size_t color = 0; color < hardwareColorCount; ++color)
                bankHistograms[bank][color] += tileHistograms[tile][color];
        }
        for (std::size_t bank = 0; bank < paletteCount; ++bank) {
            palettes[bank] = selectPalette(
                bankTileCounts[bank] == 0 ? all : bankHistograms[bank],
                background, selection, distances);
        }
        if (iteration == 5) break;
        for (std::size_t tile = 0; tile < tileHistograms.size(); ++tile) {
            std::size_t bestBank = 0;
            double bestError = paletteError(
                tileHistograms[tile], palettes[0], distances);
            for (std::size_t bank = 1; bank < paletteCount; ++bank) {
                const double error = paletteError(
                    tileHistograms[tile], palettes[bank], distances);
                if (error < bestError) {
                    bestError = error;
                    bestBank = bank;
                }
            }
            assignments[tile] = static_cast<std::uint8_t>(bestBank);
        }
    }
    return palettes;
}

IndexedTile transformed(const IndexedTile& source, bool horizontal, bool vertical)
{
    IndexedTile result{};
    for (std::size_t y = 0; y < tileSize; ++y) {
        for (std::size_t x = 0; x < tileSize; ++x) {
            const std::size_t sourceX = horizontal ? tileSize - 1U - x : x;
            const std::size_t sourceY = vertical ? tileSize - 1U - y : y;
            result[y * tileSize + x] = source[sourceY * tileSize + sourceX];
        }
    }
    return result;
}

IndexedTile canonicalPattern(const IndexedTile& source)
{
    IndexedTile result = source;
    for (const auto& flips : {std::pair{true, false}, std::pair{false, true},
                              std::pair{true, true}}) {
        const IndexedTile candidate = transformed(source, flips.first, flips.second);
        if (candidate < result) result = candidate;
    }
    return result;
}

PatternReference addPattern(const IndexedTile& tile,
                            std::vector<IndexedTile>& patterns)
{
    const IndexedTile canonical = canonicalPattern(tile);
    auto found = std::ranges::find(patterns, canonical);
    if (found == patterns.end()) {
        patterns.push_back(canonical);
        found = patterns.end() - 1;
    }
    const std::size_t index = static_cast<std::size_t>(found - patterns.begin());
    for (const auto& flips : {std::pair{false, false}, std::pair{true, false},
                              std::pair{false, true}, std::pair{true, true}}) {
        if (transformed(canonical, flips.first, flips.second) == tile)
            return {index, flips.first, flips.second};
    }
    return {index, false, false};
}

std::vector<std::uint8_t> encodePatterns(
    std::span<const IndexedTile> patterns, std::size_t byteSize)
{
    std::vector<std::uint8_t> result(byteSize);
    for (std::size_t patternIndex = 0; patternIndex < patterns.size(); ++patternIndex) {
        const auto& pattern = patterns[patternIndex];
        const std::size_t base = patternIndex * 32U;
        for (std::size_t y = 0; y < tileSize; ++y) {
            for (std::size_t pair = 0; pair < 4; ++pair) {
                result[base + y * 4U + pair] = static_cast<std::uint8_t>(
                    (pattern[y * tileSize + pair * 2U] << 4U)
                    | pattern[y * tileSize + pair * 2U + 1U]);
            }
        }
    }
    return result;
}

std::vector<std::uint8_t> encodeCram(
    const std::array<HardwarePalette, paletteCount>& palettes)
{
    std::vector<std::uint8_t> result;
    result.reserve(128);
    for (const auto& palette : palettes) {
        for (const std::uint16_t code : palette) {
            const std::uint16_t red = code & 0x07U;
            const std::uint16_t green = (code >> 3U) & 0x07U;
            const std::uint16_t blue = (code >> 6U) & 0x07U;
            const std::uint16_t word = static_cast<std::uint16_t>(
                (blue << 9U) | (green << 5U) | (red << 1U));
            result.push_back(static_cast<std::uint8_t>(word >> 8U));
            result.push_back(static_cast<std::uint8_t>(word & 0xffU));
        }
    }
    return result;
}

std::vector<std::uint8_t> displayRegisters(ConversionMode mode)
{
    std::vector<std::uint8_t> result(registerCount);
    const bool wide = isH40(mode);
    result[0] = 0x04U;
    result[1] = static_cast<std::uint8_t>(0x74U | (isPal30(mode) ? 0x08U : 0U));
    result[2] = 0x30U; // Scroll A at $C000.
    result[3] = 0x2cU; // Window at $B000.
    result[4] = 0x07U; // Scroll B at $E000.
    result[5] = wide ? 0x54U : 0x5fU; // SAT at $A800 or $BE00.
    result[7] = 0x00U;
    result[10] = 0xffU;
    result[11] = 0x00U;
    result[12] = wide ? 0x81U : 0x00U;
    result[13] = wide ? 0x2bU : 0x2eU; // H scroll at $AC00 or $B800.
    result[15] = 0x02U;
    result[16] = wide ? 0x01U : 0x00U; // 64x32 or 32x32 Scroll A map.
    return result;
}

} // namespace

ConversionResult convertSegaGenesisMode5(const RgbImage& source,
                                          const ConversionSettings& settings,
                                          CancellationToken cancellation,
                                          ConversionProgressCallback progress)
{
    if (!isGenesisMode(settings.mode)) {
        return failure("genesis-mode5-wrong-mode",
                       "The Sega Genesis converter requires a Mode V display mode.");
    }
    if (!validate(settings).empty()) {
        return failure("genesis-mode5-invalid-settings",
                       "Sega Genesis conversion settings are invalid.");
    }
    const auto& descriptor = displayMode(settings.mode);
    if (source.width() != descriptor.geometry.width
        || source.height() != descriptor.geometry.height) {
        return failure("genesis-mode5-invalid-dimensions",
                       "The source geometry does not match the selected Genesis mode.");
    }
    const auto dithering = ditherConfiguration(
        settings.dither, settings.errorDistribution);
    if (!dithering) {
        return failure("genesis-mode5-invalid-dither",
                       "The Genesis converter received an unsupported dither mode.");
    }
    if (cancellation.isCancellationRequested()) return cancelled();

    const std::size_t visibleColumns = source.width() / tileSize;
    const std::size_t visibleRows = source.height() / tileSize;
    const std::size_t tileCount = visibleColumns * visibleRows;
    std::vector<ColorHistogram> tileHistograms(tileCount);
    for (std::uint32_t y = 0; y < source.height(); ++y) {
        for (std::uint32_t x = 0; x < source.width(); ++x) {
            const std::size_t tile = static_cast<std::size_t>(y / tileSize)
                * visibleColumns + x / tileSize;
            ++tileHistograms[tile][hardwareCode(sourcePixel(source, x, y))];
        }
    }

    const HardwareDistances hardwareDistances = makeHardwareDistances(settings);
    std::vector<std::uint8_t> assignments;
    const auto palettes = selectPaletteBanks(
        tileHistograms, settings.paletteSelection, hardwareDistances, assignments);
    std::array<std::array<RgbColor, colorsPerPalette>, paletteCount> decoded{};
    std::array<std::array<PreparedColorSample, colorsPerPalette>, paletteCount> prepared{};
    const ColorDistanceEvaluator evaluator(settings);
    for (std::size_t bank = 0; bank < paletteCount; ++bank) {
        for (std::size_t entry = 0; entry < colorsPerPalette; ++entry) {
            decoded[bank][entry] = hardwareColor(palettes[bank][entry]);
            prepared[bank][entry] = evaluator.prepare(RgbSample(decoded[bank][entry]));
        }
    }

    auto preview = RgbImage::createTightlyPacked(
        source.width(), source.height(), source.pixelFormat());
    if (!preview) {
        return failure("genesis-mode5-preview-allocation",
                       "The Genesis preview could not be allocated.");
    }
    std::vector<Tile> tiles(tileCount);
    for (std::size_t tile = 0; tile < tileCount; ++tile)
        tiles[tile].paletteBank = assignments[tile];

    std::optional<ErrorDiffusionBuffer> errors;
    if (dithering->distributeError) {
        errors = ErrorDiffusionBuffer::create(source.width(), source.height());
        if (!errors) {
            return failure("genesis-mode5-error-buffer",
                           "The Genesis error-diffusion buffer could not be allocated.");
        }
    }
    for (std::uint32_t y = 0; y < source.height(); ++y) {
        if (cancellation.isCancellationRequested()) return cancelled();
        for (std::uint32_t x = 0; x < source.width(); ++x) {
            const std::size_t tileIndex = static_cast<std::size_t>(y / tileSize)
                * visibleColumns + x / tileSize;
            const std::uint8_t bank = tiles[tileIndex].paletteBank;
            RgbSample sample(sourcePixel(source, x, y));
            if (dithering->ordered) {
                sample = *applyOrderedDither(sample, x, y,
                                             settings.orderedDitherMapSize,
                                             settings.orderedDitherBrightness);
            }
            if (errors)
                sample = errors->adjustedSample(sample, x, y, settings.errorAccumulation);
            std::uint8_t nearest = 0;
            double nearestDistance = std::numeric_limits<double>::max();
            for (std::size_t entry = 0; entry < colorsPerPalette; ++entry) {
                const double distance = evaluator.distanceSquared(
                    sample, prepared[bank][entry]);
                if (distance < nearestDistance) {
                    nearestDistance = distance;
                    nearest = static_cast<std::uint8_t>(entry);
                }
            }
            tiles[tileIndex].pixels[static_cast<std::size_t>(y % tileSize)
                                         * tileSize + x % tileSize] = nearest;
            const RgbColor color = decoded[bank][nearest];
            setPixel(*preview, x, y, color);
            if (errors) {
                errors->distribute(x, y,
                                   {sample.red - color.red,
                                    sample.green - color.green,
                                    sample.blue - color.blue},
                                   dithering->kernel);
            }
        }
        if (progress && ((y + 1U) % 8U == 0U || y + 1U == source.height()))
            progress(*preview, y + 1U, source.height());
    }

    const std::size_t mapColumns = isH40(settings.mode) ? 64U : 32U;
    const std::size_t mapBytes = mapColumns * 32U * 2U;
    const std::size_t patternBytes = isH40(settings.mode) ? 0xa800U : 0xb000U;
    const std::size_t patternLimit = patternBytes / 32U;
    std::vector<std::uint8_t> nameTable(mapBytes);
    std::vector<IndexedTile> patterns;
    patterns.reserve(tileCount);
    for (std::size_t tileIndex = 0; tileIndex < tiles.size(); ++tileIndex) {
        if (cancellation.isCancellationRequested()) return cancelled();
        const PatternReference reference = addPattern(tiles[tileIndex].pixels, patterns);
        if (patterns.size() > patternLimit) {
            return failure("genesis-mode5-pattern-limit",
                           "The converted image exceeds the standard Mode V VRAM tile budget.");
        }
        std::uint16_t word = static_cast<std::uint16_t>(reference.index);
        if (reference.horizontalFlip) word |= 1U << 11U;
        if (reference.verticalFlip) word |= 1U << 12U;
        word |= static_cast<std::uint16_t>(tiles[tileIndex].paletteBank) << 13U;
        const std::size_t visibleX = tileIndex % visibleColumns;
        const std::size_t visibleY = tileIndex / visibleColumns;
        const std::size_t mapOffset = (visibleY * mapColumns + visibleX) * 2U;
        nameTable[mapOffset] = static_cast<std::uint8_t>(word >> 8U);
        nameTable[mapOffset + 1U] = static_cast<std::uint8_t>(word & 0xffU);
    }
    if (progress) progress(*preview, source.height(), source.height());

    TargetMemoryImage target{
        .profile = TargetProfileId::SegaGenesis,
        .mode = settings.mode,
        .palette = std::nullopt,
        .tables = {
            {TargetTableRole::Pattern, encodePatterns(patterns, patternBytes)},
            {TargetTableRole::TileMap, std::move(nameTable)},
            {TargetTableRole::Palette, encodeCram(palettes)},
            {TargetTableRole::DisplayRegisters, displayRegisters(settings.mode)},
        },
    };
    return {
        .status = ConversionStatus::Succeeded,
        .preview = std::move(preview),
        .diagnostics = {{DiagnosticSeverity::Information,
                         "genesis-mode5-plane-a",
                         "The Screen Image is emitted as one opaque Scroll A plane; "
                         "Scroll B, Window, sprites, and interlaced Mode 2 remain separate authoring concerns."}},
        .target = std::move(target),
    };
}

} // namespace retrovdp::core
