#include "retrovdp/core/SegaSmsVdpConverter.hpp"

#include "retrovdp/core/ColorMath.hpp"
#include "retrovdp/core/Dithering.hpp"
#include "retrovdp/core/TargetProfile.hpp"
#include "retrovdp/core/Validation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
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

constexpr std::uint32_t screenWidth = 256;
constexpr std::uint32_t tileSize = 8;
constexpr std::uint32_t mapColumns = screenWidth / tileSize;
constexpr std::size_t hardwareColorCount = 64;
constexpr std::size_t colorsPerPalette = 16;
constexpr std::size_t tilePixelCount = tileSize * tileSize;
constexpr std::size_t nameTableBytes = 2048;
constexpr std::size_t registerCount = 11;

using ColorHistogram = std::array<std::uint32_t, hardwareColorCount>;
using HardwarePalette = std::array<std::uint8_t, colorsPerPalette>;
using IndexedTile = std::array<std::uint8_t, tilePixelCount>;
using HardwareDistances =
    std::array<std::array<double, hardwareColorCount>, hardwareColorCount>;

struct Tile {
    IndexedTile pixels{};
    std::uint8_t paletteBank{};
};

struct PatternMatch {
    std::size_t index{};
    bool horizontalFlip{};
    bool verticalFlip{};
    double error{std::numeric_limits<double>::max()};
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
    return {
        .status = ConversionStatus::Cancelled,
        .preview = std::nullopt,
        .diagnostics = {},
        .target = std::nullopt,
    };
}

bool isSmsMode(ConversionMode mode)
{
    return mode == ConversionMode::Mode4Sms192
        || mode == ConversionMode::Mode4Sms224
        || mode == ConversionMode::Mode4Sms240;
}

std::uint8_t to2(std::uint8_t value)
{
    return static_cast<std::uint8_t>(
        (static_cast<unsigned>(value) * 3U + 127U) / 255U);
}

std::uint8_t hardwareCode(RgbColor color)
{
    return static_cast<std::uint8_t>(to2(color.red)
        | (to2(color.green) << 2U) | (to2(color.blue) << 4U));
}

RgbColor hardwareColor(std::uint8_t code)
{
    return {
        static_cast<std::uint8_t>((code & 0x03U) * 85U),
        static_cast<std::uint8_t>(((code >> 2U) & 0x03U) * 85U),
        static_cast<std::uint8_t>(((code >> 4U) & 0x03U) * 85U),
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

HardwareDistances makeHardwareDistances(const ConversionSettings& settings)
{
    HardwareDistances result{};
    for (std::size_t left = 0; left < hardwareColorCount; ++left) {
        for (std::size_t right = 0; right < hardwareColorCount; ++right) {
            result[left][right] = colorDistanceSquared(
                RgbSample(hardwareColor(static_cast<std::uint8_t>(left))),
                RgbSample(hardwareColor(static_cast<std::uint8_t>(right))), settings);
        }
    }
    return result;
}

HardwarePalette selectPalette(const ColorHistogram& histogram,
                              PaletteSelectionMode selection,
                              const HardwareDistances& distances)
{
    HardwarePalette result{};
    std::array<bool, hardwareColorCount> selected{};
    const auto mostPopular = [&histogram, &selected] {
        std::size_t best = 0;
        for (std::size_t color = 1; color < hardwareColorCount; ++color) {
            if (!selected[color]
                && (selected[best] || histogram[color] > histogram[best])) {
                best = color;
            }
        }
        return best;
    };

    if (selection == PaletteSelectionMode::Popularity) {
        for (std::size_t entry = 0; entry < result.size(); ++entry) {
            const std::size_t color = mostPopular();
            result[entry] = static_cast<std::uint8_t>(color);
            selected[color] = true;
        }
        return result;
    }

    std::size_t first = 0;
    for (std::size_t color = 1; color < hardwareColorCount; ++color) {
        if (histogram[color] > histogram[first]) first = color;
    }
    result[0] = static_cast<std::uint8_t>(first);
    selected[first] = true;
    std::array<double, hardwareColorCount> nearest{};
    for (std::size_t color = 0; color < hardwareColorCount; ++color)
        nearest[color] = distances[color][first];

    for (std::size_t entry = 1; entry < result.size(); ++entry) {
        std::size_t best = 0;
        long double bestGain = -1.0;
        for (std::size_t candidate = 0; candidate < hardwareColorCount; ++candidate) {
            if (selected[candidate]) continue;
            long double gain = 0.0;
            for (std::size_t color = 0; color < hardwareColorCount; ++color) {
                if (histogram[color] == 0) continue;
                gain += static_cast<long double>(histogram[color])
                    * std::max(0.0, nearest[color] - distances[color][candidate]);
            }
            if (gain > bestGain) {
                bestGain = gain;
                best = candidate;
            }
        }
        result[entry] = static_cast<std::uint8_t>(best);
        selected[best] = true;
        for (std::size_t color = 0; color < hardwareColorCount; ++color)
            nearest[color] = std::min(nearest[color], distances[color][best]);
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
        for (const std::uint8_t entry : palette)
            nearest = std::min(nearest, distances[color][entry]);
        result += nearest * histogram[color];
    }
    return result;
}

std::array<HardwarePalette, 2>
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
    std::array<HardwarePalette, 2> palettes{};
    palettes[0] = selectPalette(all, selection, distances);

    std::vector<std::pair<double, std::size_t>> errors;
    errors.reserve(tileHistograms.size());
    for (std::size_t tile = 0; tile < tileHistograms.size(); ++tile)
        errors.emplace_back(paletteError(tileHistograms[tile], palettes[0], distances), tile);
    std::ranges::sort(errors, std::greater<>{});
    ColorHistogram second{};
    for (std::size_t rank = 0; rank < (errors.size() + 1U) / 2U; ++rank) {
        const auto& histogram = tileHistograms[errors[rank].second];
        for (std::size_t color = 0; color < hardwareColorCount; ++color)
            second[color] += histogram[color];
    }
    palettes[1] = selectPalette(second, selection, distances);

    assignments.assign(tileHistograms.size(), 0);
    for (int iteration = 0; iteration < 4; ++iteration) {
        std::array<ColorHistogram, 2> bankHistograms{};
        std::array<std::size_t, 2> bankTileCounts{};
        for (std::size_t tile = 0; tile < tileHistograms.size(); ++tile) {
            const double firstError = paletteError(
                tileHistograms[tile], palettes[0], distances);
            const double secondError = paletteError(
                tileHistograms[tile], palettes[1], distances);
            const std::uint8_t bank = secondError < firstError ? 1U : 0U;
            assignments[tile] = bank;
            ++bankTileCounts[bank];
            for (std::size_t color = 0; color < hardwareColorCount; ++color)
                bankHistograms[bank][color] += tileHistograms[tile][color];
        }
        for (std::size_t bank = 0; bank < palettes.size(); ++bank) {
            if (bankTileCounts[bank] != 0)
                palettes[bank] = selectPalette(bankHistograms[bank], selection, distances);
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
    for (const auto& [horizontal, vertical] :
         {std::pair{true, false}, std::pair{false, true}, std::pair{true, true}}) {
        const IndexedTile candidate = transformed(source, horizontal, vertical);
        if (candidate < result) result = candidate;
    }
    return result;
}

double patternError(const Tile& tile,
                    const IndexedTile& pattern,
                    bool horizontal,
                    bool vertical,
                    const std::array<std::array<std::array<double, 16>, 16>, 2>& distances)
{
    double result = 0.0;
    for (std::size_t y = 0; y < tileSize; ++y) {
        for (std::size_t x = 0; x < tileSize; ++x) {
            const std::size_t patternX = horizontal ? tileSize - 1U - x : x;
            const std::size_t patternY = vertical ? tileSize - 1U - y : y;
            result += distances[tile.paletteBank]
                [tile.pixels[y * tileSize + x]]
                [pattern[patternY * tileSize + patternX]];
        }
    }
    return result;
}

PatternMatch bestPatternMatch(
    const Tile& tile,
    std::span<const IndexedTile> patterns,
    const std::array<std::array<std::array<double, 16>, 16>, 2>& distances)
{
    PatternMatch best;
    for (std::size_t index = 0; index < patterns.size(); ++index) {
        for (const auto& [horizontal, vertical] :
             {std::pair{false, false}, std::pair{true, false},
              std::pair{false, true}, std::pair{true, true}}) {
            const double error = patternError(
                tile, patterns[index], horizontal, vertical, distances);
            if (error < best.error) {
                best = {index, horizontal, vertical, error};
                if (error == 0.0) break;
            }
        }
        if (best.error == 0.0) break;
    }
    return best;
}

std::vector<IndexedTile> choosePatterns(
    const std::vector<Tile>& tiles,
    std::size_t limit,
    const std::array<std::array<std::array<double, 16>, 16>, 2>& distances,
    CancellationToken cancellation,
    bool& reduced)
{
    std::vector<IndexedTile> unique;
    std::vector<std::size_t> frequency;
    for (const Tile& tile : tiles) {
        const IndexedTile canonical = canonicalPattern(tile.pixels);
        const auto found = std::ranges::find(unique, canonical);
        if (found == unique.end()) {
            unique.push_back(canonical);
            frequency.push_back(1);
        } else {
            ++frequency[static_cast<std::size_t>(found - unique.begin())];
        }
    }
    reduced = unique.size() > limit;
    if (!reduced) return unique;

    const std::size_t first = static_cast<std::size_t>(
        std::max_element(frequency.begin(), frequency.end()) - frequency.begin());
    std::vector<IndexedTile> selected{unique[first]};
    std::vector<double> nearest(tiles.size(), std::numeric_limits<double>::max());
    std::vector<bool> selectedUnique(unique.size());
    selectedUnique[first] = true;

    while (selected.size() < limit) {
        if (cancellation.isCancellationRequested()) return {};
        const IndexedTile& newest = selected.back();
        double farthestError = -1.0;
        std::size_t farthestTile = 0;
        for (std::size_t tileIndex = 0; tileIndex < tiles.size(); ++tileIndex) {
            const double error = bestPatternMatch(
                tiles[tileIndex], std::span(&newest, 1U), distances).error;
            nearest[tileIndex] = std::min(nearest[tileIndex], error);
            if (nearest[tileIndex] > farthestError) {
                farthestError = nearest[tileIndex];
                farthestTile = tileIndex;
            }
        }
        if (farthestError <= 0.0) break;
        const IndexedTile next = canonicalPattern(tiles[farthestTile].pixels);
        const auto found = std::ranges::find(unique, next);
        if (found == unique.end()) break;
        const std::size_t uniqueIndex = static_cast<std::size_t>(found - unique.begin());
        if (selectedUnique[uniqueIndex]) {
            nearest[farthestTile] = 0.0;
            continue;
        }
        selectedUnique[uniqueIndex] = true;
        selected.push_back(next);
    }
    return selected;
}

std::vector<std::uint8_t> encodePatterns(std::span<const IndexedTile> patterns,
                                         std::size_t byteSize)
{
    std::vector<std::uint8_t> result(byteSize);
    for (std::size_t patternIndex = 0; patternIndex < patterns.size(); ++patternIndex) {
        const auto& pattern = patterns[patternIndex];
        const std::size_t base = patternIndex * 32U;
        for (std::size_t y = 0; y < tileSize; ++y) {
            for (std::size_t plane = 0; plane < 4; ++plane) {
                std::uint8_t byte = 0;
                for (std::size_t x = 0; x < tileSize; ++x) {
                    if ((pattern[y * tileSize + x] & (1U << plane)) != 0U)
                        byte = static_cast<std::uint8_t>(byte | (0x80U >> x));
                }
                result[base + y * 4U + plane] = byte;
            }
        }
    }
    return result;
}

std::vector<std::uint8_t> encodeCram(
    const std::array<HardwarePalette, 2>& palettes)
{
    std::vector<std::uint8_t> result;
    result.reserve(32);
    for (const auto& palette : palettes)
        result.insert(result.end(), palette.begin(), palette.end());
    return result;
}

std::vector<std::uint8_t> displayRegisters(ConversionMode mode)
{
    std::uint8_t register0 = 0x04U;
    std::uint8_t register1 = 0xc0U;
    if (mode == ConversionMode::Mode4Sms224) register0 |= 0x02U;
    if (mode == ConversionMode::Mode4Sms240) register1 |= 0x08U;
    return {register0, register1, 0xffU, 0xffU, 0xffU, 0xffU,
            0xfbU, 0xf0U, 0x00U, 0x00U, 0xffU};
}

} // namespace

ConversionResult convertSegaSmsMode4(const RgbImage& source,
                                     const ConversionSettings& settings,
                                     CancellationToken cancellation,
                                     ConversionProgressCallback progress)
{
    if (!isSmsMode(settings.mode)) {
        return failure("sms-mode4-wrong-mode",
                       "The Sega Master System converter requires a Mode 4 display mode.");
    }
    if (!validate(settings).empty()) {
        return failure("sms-mode4-invalid-settings",
                       "Sega Master System conversion settings are invalid.");
    }
    const auto& descriptor = displayMode(settings.mode);
    if (source.width() != descriptor.geometry.width
        || source.height() != descriptor.geometry.height) {
        return failure("sms-mode4-invalid-dimensions",
                       "The source geometry does not match the selected Master System mode.");
    }
    const auto dithering = ditherConfiguration(settings.dither, settings.errorDistribution);
    if (!dithering) {
        return failure("sms-mode4-invalid-dither",
                       "The Master System converter received an unsupported dither mode.");
    }
    if (cancellation.isCancellationRequested()) return cancelled();

    const std::size_t tileRows = source.height() / tileSize;
    const std::size_t tileCount = mapColumns * tileRows;
    std::vector<ColorHistogram> tileHistograms(tileCount);
    for (std::uint32_t y = 0; y < source.height(); ++y) {
        for (std::uint32_t x = 0; x < source.width(); ++x) {
            const std::size_t tile = static_cast<std::size_t>(y / tileSize)
                * mapColumns + x / tileSize;
            ++tileHistograms[tile][hardwareCode(sourcePixel(source, x, y))];
        }
    }

    const HardwareDistances hardwareDistances = makeHardwareDistances(settings);
    std::vector<std::uint8_t> assignments;
    const auto palettes = selectPaletteBanks(
        tileHistograms, settings.paletteSelection, hardwareDistances, assignments);
    std::array<std::array<RgbColor, colorsPerPalette>, 2> decodedPalettes{};
    std::array<std::array<PreparedColorSample, colorsPerPalette>, 2> preparedPalettes{};
    const ColorDistanceEvaluator evaluator(settings);
    for (std::size_t bank = 0; bank < palettes.size(); ++bank) {
        for (std::size_t entry = 0; entry < colorsPerPalette; ++entry) {
            decodedPalettes[bank][entry] = hardwareColor(palettes[bank][entry]);
            preparedPalettes[bank][entry] = evaluator.prepare(
                RgbSample(decodedPalettes[bank][entry]));
        }
    }

    auto preview = RgbImage::createTightlyPacked(
        source.width(), source.height(), source.pixelFormat());
    if (!preview) {
        return failure("sms-mode4-preview-allocation",
                       "The Master System preview could not be allocated.");
    }
    std::vector<Tile> tiles(tileCount);
    for (std::size_t tile = 0; tile < tileCount; ++tile)
        tiles[tile].paletteBank = assignments[tile];

    std::optional<ErrorDiffusionBuffer> errors;
    if (dithering->distributeError) {
        errors = ErrorDiffusionBuffer::create(source.width(), source.height());
        if (!errors) {
            return failure("sms-mode4-error-buffer",
                           "The Master System error-diffusion buffer could not be allocated.");
        }
    }
    for (std::uint32_t y = 0; y < source.height(); ++y) {
        if (cancellation.isCancellationRequested()) return cancelled();
        for (std::uint32_t x = 0; x < source.width(); ++x) {
            const std::size_t tileIndex = static_cast<std::size_t>(y / tileSize)
                * mapColumns + x / tileSize;
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
                    sample, preparedPalettes[bank][entry]);
                if (distance < nearestDistance) {
                    nearestDistance = distance;
                    nearest = static_cast<std::uint8_t>(entry);
                }
            }
            const std::size_t pixelIndex = static_cast<std::size_t>(y % tileSize)
                * tileSize + x % tileSize;
            tiles[tileIndex].pixels[pixelIndex] = nearest;
            const RgbColor color = decodedPalettes[bank][nearest];
            setPixel(*preview, x, y, color);
            if (errors) {
                errors->distribute(x, y,
                                   {sample.red - color.red,
                                    sample.green - color.green,
                                    sample.blue - color.blue},
                                   dithering->kernel);
            }
        }
        const std::uint32_t completedRows = y + 1U;
        if (progress && (completedRows % 8U == 0U
                         || completedRows == source.height())) {
            progress(*preview, completedRows, source.height());
        }
    }

    std::array<std::array<std::array<double, 16>, 16>, 2> paletteDistances{};
    for (std::size_t bank = 0; bank < palettes.size(); ++bank) {
        for (std::size_t left = 0; left < colorsPerPalette; ++left) {
            for (std::size_t right = 0; right < colorsPerPalette; ++right) {
                paletteDistances[bank][left][right] =
                    hardwareDistances[palettes[bank][left]][palettes[bank][right]];
            }
        }
    }

    const std::size_t patternBytes = settings.mode == ConversionMode::Mode4Sms192
        ? 0x3800U : 0x3700U;
    const std::size_t patternLimit = patternBytes / 32U;
    bool patternsReduced = false;
    std::vector<IndexedTile> patterns = choosePatterns(
        tiles, patternLimit, paletteDistances, cancellation, patternsReduced);
    if (cancellation.isCancellationRequested()) return cancelled();
    if (patterns.empty()) {
        return failure("sms-mode4-pattern-selection",
                       "The Master System pattern dictionary could not be generated.");
    }

    std::vector<std::uint8_t> nameTable(nameTableBytes);
    for (std::size_t tileIndex = 0; tileIndex < tiles.size(); ++tileIndex) {
        if (cancellation.isCancellationRequested()) return cancelled();
        const PatternMatch match = bestPatternMatch(
            tiles[tileIndex], patterns, paletteDistances);
        std::uint16_t word = static_cast<std::uint16_t>(match.index);
        if (match.horizontalFlip) word |= 1U << 9U;
        if (match.verticalFlip) word |= 1U << 10U;
        if (tiles[tileIndex].paletteBank != 0U) word |= 1U << 11U;
        nameTable[tileIndex * 2U] = static_cast<std::uint8_t>(word & 0xffU);
        nameTable[tileIndex * 2U + 1U] = static_cast<std::uint8_t>(word >> 8U);

        const IndexedTile displayed = transformed(
            patterns[match.index], match.horizontalFlip, match.verticalFlip);
        const std::uint32_t tileX = static_cast<std::uint32_t>(tileIndex % mapColumns);
        const std::uint32_t tileY = static_cast<std::uint32_t>(tileIndex / mapColumns);
        for (std::uint32_t y = 0; y < tileSize; ++y) {
            for (std::uint32_t x = 0; x < tileSize; ++x) {
                setPixel(*preview, tileX * tileSize + x, tileY * tileSize + y,
                         decodedPalettes[tiles[tileIndex].paletteBank]
                                        [displayed[y * tileSize + x]]);
            }
        }
    }
    if (progress) progress(*preview, source.height(), source.height());

    std::vector<ConversionDiagnostic> diagnostics;
    if (patternsReduced) {
        diagnostics.push_back({
            DiagnosticSeverity::Information,
            "sms-mode4-pattern-limit",
            "The source required more unique tiles than fit beside the hardware name table; "
            "the closest reusable patterns were selected.",
        });
    }
    TargetMemoryImage target{
        .profile = TargetProfileId::SegaMasterSystem,
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
        .diagnostics = std::move(diagnostics),
        .target = std::move(target),
    };
}

} // namespace retrovdp::core
