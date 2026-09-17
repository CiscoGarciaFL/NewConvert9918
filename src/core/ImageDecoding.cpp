#include "newconvert9918/core/ImageDecoding.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <vector>

namespace newconvert9918::core {
namespace {

constexpr std::size_t graphicsTableSize = 6144;
constexpr std::uint32_t screenWidth = 256;
constexpr std::uint32_t screenHeight = 192;

constexpr std::array<std::array<std::uint8_t, 3>, 16> hardwarePalette{{
    {0, 0, 0},       {0, 0, 0},       {32, 200, 64},   {88, 216, 120},
    {80, 80, 232},   {120, 112, 248}, {208, 80, 72},   {64, 232, 240},
    {248, 80, 80},   {248, 120, 120}, {208, 192, 80},  {224, 200, 128},
    {32, 176, 56},   {200, 88, 184},  {200, 200, 200}, {248, 248, 248},
}};

ImageDecodeResult failure(ImageDecodeError error, std::string message)
{
    return {.image = std::nullopt, .error = error, .message = std::move(message)};
}

ImageDecodeResult success(RgbImage image)
{
    return {.image = std::move(image), .error = ImageDecodeError::None, .message = {}};
}

std::uint16_t little16(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(bytes[offset + 1] << 8);
}

bool decodePcxRle(std::span<const std::uint8_t> encoded,
                  bool compressed,
                  std::size_t required,
                  std::vector<std::uint8_t>& decoded)
{
    decoded.reserve(required);
    std::size_t cursor = 0;
    while (decoded.size() < required && cursor < encoded.size()) {
        std::uint8_t value = encoded[cursor++];
        std::size_t count = 1;
        if (compressed && (value & 0xc0U) == 0xc0U) {
            count = value & 0x3fU;
            if (count == 0 || cursor >= encoded.size()) {
                return false;
            }
            value = encoded[cursor++];
        }
        if (count > required - decoded.size()) {
            return false;
        }
        decoded.insert(decoded.end(), count, value);
    }
    return decoded.size() == required;
}

std::array<std::uint8_t, 3> f18Color(std::span<const std::uint8_t> palette,
                                    std::size_t offset,
                                    std::uint8_t index)
{
    const std::size_t entry = offset + static_cast<std::size_t>(index) * 2;
    return {
        static_cast<std::uint8_t>((palette[entry] & 0x0fU) * 17U),
        static_cast<std::uint8_t>((palette[entry + 1] >> 4) * 17U),
        static_cast<std::uint8_t>((palette[entry + 1] & 0x0fU) * 17U),
    };
}

std::array<std::uint8_t, 3> average(std::array<std::uint8_t, 3> first,
                                   std::array<std::uint8_t, 3> second)
{
    return {
        static_cast<std::uint8_t>((static_cast<unsigned>(first[0]) + second[0]) / 2U),
        static_cast<std::uint8_t>((static_cast<unsigned>(first[1]) + second[1]) / 2U),
        static_cast<std::uint8_t>((static_cast<unsigned>(first[2]) + second[2]) / 2U),
    };
}

std::uint8_t halfUnderlay(std::span<const std::uint8_t> table,
                          std::uint32_t x,
                          std::uint32_t y)
{
    const std::size_t bitmapGroup = (y / 8U) / 8U;
    const std::size_t multicolorGroup = (y / 8U) % 4U;
    std::size_t character = (y / 8U) * 32U + x / 8U;
    character = (character + 32U * bitmapGroup) & 0xffU;
    const std::size_t address = character * 8U + 2U * multicolorGroup + ((y % 8U) >= 4U);
    const std::uint8_t pair = table[address];
    return (x % 8U) < 4U ? pair >> 4 : pair & 0x0fU;
}

} // namespace

ImageDecodeResult decodePcx(std::span<const std::uint8_t> bytes,
                            const ImageSizeLimits& limits)
{
    if (bytes.size() < 128 || bytes[0] != 0x0aU) {
        return failure(ImageDecodeError::InvalidHeader, "PCX header is missing or invalid.");
    }
    if (bytes[2] > 1U) {
        return failure(ImageDecodeError::UnsupportedVariant, "PCX encoding is not supported.");
    }

    const std::uint8_t bitsPerPlane = bytes[3];
    const std::uint8_t planeCount = bytes[65];
    const std::uint16_t xMin = little16(bytes, 4);
    const std::uint16_t yMin = little16(bytes, 6);
    const std::uint16_t xMax = little16(bytes, 8);
    const std::uint16_t yMax = little16(bytes, 10);
    const std::uint16_t bytesPerLine = little16(bytes, 66);
    if (xMax < xMin || yMax < yMin || bytesPerLine == 0 || planeCount == 0) {
        return failure(ImageDecodeError::MalformedData, "PCX geometry is invalid.");
    }
    const std::uint32_t width = static_cast<std::uint32_t>(xMax - xMin) + 1U;
    const std::uint32_t height = static_cast<std::uint32_t>(yMax - yMin) + 1U;
    const bool bitPlanes = bitsPerPlane == 1U && planeCount <= 4U;
    const bool bytePlanes = bitsPerPlane == 8U
        && (planeCount == 1U || planeCount == 3U || planeCount == 4U);
    if (!bitPlanes && !bytePlanes) {
        return failure(ImageDecodeError::UnsupportedVariant,
                       "PCX bit depth or plane count is not supported.");
    }
    const std::size_t minimumLineBytes = bitPlanes ? (width + 7U) / 8U : width;
    if (bytesPerLine < minimumLineBytes) {
        return failure(ImageDecodeError::MalformedData, "PCX scanline stride is too small.");
    }
    if (height > std::numeric_limits<std::size_t>::max()
            / (static_cast<std::size_t>(bytesPerLine) * planeCount)) {
        return failure(ImageDecodeError::AllocationLimitExceeded, "PCX decoded size overflows.");
    }
    const std::size_t decodedSize = static_cast<std::size_t>(height)
        * bytesPerLine * planeCount;
    if (decodedSize > limits.maximumBytes) {
        return failure(ImageDecodeError::AllocationLimitExceeded,
                       "PCX decoded data exceeds the configured byte limit.");
    }

    const bool indexed256 = bitsPerPlane == 8U && planeCount == 1U;
    std::span<const std::uint8_t> palette;
    std::size_t encodedEnd = bytes.size();
    if (indexed256) {
        if (bytes.size() < 897 || bytes[bytes.size() - 769] != 0x0cU) {
            return failure(ImageDecodeError::TruncatedData,
                           "8-bit PCX palette is missing or truncated.");
        }
        palette = bytes.subspan(bytes.size() - 768, 768);
        encodedEnd = bytes.size() - 769;
    }

    std::vector<std::uint8_t> decoded;
    if (!decodePcxRle(bytes.subspan(128, encodedEnd - 128),
                      bytes[2] == 1U,
                      decodedSize,
                      decoded)) {
        return failure(ImageDecodeError::TruncatedData,
                       "PCX pixel data is truncated or malformed.");
    }

    const PixelFormat format = planeCount == 4U && bitsPerPlane == 8U
        ? PixelFormat::Rgba8888 : PixelFormat::Rgb888;
    auto image = RgbImage::createTightlyPacked(width, height, format, limits);
    if (!image) {
        return failure(ImageDecodeError::AllocationLimitExceeded,
                       "PCX image exceeds the configured image limits.");
    }
    const std::size_t channels = bytesPerPixel(format);
    const std::size_t scanlineBytes = static_cast<std::size_t>(bytesPerLine) * planeCount;
    for (std::uint32_t y = 0; y < height; ++y) {
        auto output = image->row(y);
        const std::uint8_t* scanline = decoded.data() + static_cast<std::size_t>(y) * scanlineBytes;
        for (std::uint32_t x = 0; x < width; ++x) {
            std::array<std::uint8_t, 4> pixel{0, 0, 0, 255};
            if (indexed256) {
                const std::size_t index = static_cast<std::size_t>(scanline[x]) * 3U;
                std::copy_n(palette.data() + index, 3, pixel.begin());
            } else if (bitPlanes) {
                std::uint8_t index = 0;
                for (std::uint8_t plane = 0; plane < planeCount; ++plane) {
                    const std::uint8_t value = scanline[static_cast<std::size_t>(plane)
                        * bytesPerLine + x / 8U];
                    index |= static_cast<std::uint8_t>(((value >> (7U - x % 8U)) & 1U) << plane);
                }
                const std::size_t paletteOffset = 16U + static_cast<std::size_t>(index) * 3U;
                std::copy_n(bytes.data() + paletteOffset, 3, pixel.begin());
            } else {
                for (std::uint8_t plane = 0; plane < planeCount; ++plane) {
                    pixel[plane] = scanline[static_cast<std::size_t>(plane) * bytesPerLine + x];
                }
            }
            std::copy_n(pixel.begin(), channels,
                        output.begin() + static_cast<std::size_t>(x) * channels);
        }
    }
    return success(std::move(*image));
}

ImageDecodeResult decodeGraphics2(std::span<const std::uint8_t> patternData,
                                  std::span<const std::uint8_t> colorData,
                                  std::span<const std::uint8_t> paletteData,
                                  std::span<const std::uint8_t> multicolorData,
                                  const ImageSizeLimits& limits)
{
    if (patternData.size() != graphicsTableSize || colorData.size() != graphicsTableSize) {
        return failure(ImageDecodeError::TruncatedData,
                       "Graphics II pattern and color tables must each contain 6144 bytes.");
    }
    if (!paletteData.empty() && paletteData.size() != 32U
        && paletteData.size() != graphicsTableSize) {
        return failure(ImageDecodeError::UnsupportedVariant,
                       "F18A palette data must contain 32 or 6144 bytes.");
    }
    if (!multicolorData.empty() && multicolorData.size() != 2048U) {
        return failure(ImageDecodeError::UnsupportedVariant,
                       "Half Multicolor data must contain 2048 bytes.");
    }
    auto image = RgbImage::createTightlyPacked(
        screenWidth, screenHeight, PixelFormat::Rgb888, limits);
    if (!image) {
        return failure(ImageDecodeError::AllocationLimitExceeded,
                       "Retro image exceeds the configured image limits.");
    }
    for (std::uint32_t y = 0; y < screenHeight; ++y) {
        auto row = image->row(y);
        for (std::uint32_t cellX = 0; cellX < 32U; ++cellX) {
            const std::size_t tableOffset = static_cast<std::size_t>(y / 8U) * 256U
                + cellX * 8U + y % 8U;
            const std::uint8_t pattern = patternData[tableOffset];
            const std::uint8_t colors = colorData[tableOffset];
            for (std::uint32_t bit = 0; bit < 8U; ++bit) {
                const std::uint32_t x = cellX * 8U + bit;
                const std::uint8_t index = (pattern & (0x80U >> bit)) != 0
                    ? colors >> 4 : colors & 0x0fU;
                std::array<std::uint8_t, 3> color{};
                if (paletteData.empty()) {
                    color = hardwarePalette[index];
                } else {
                    const std::size_t paletteOffset = paletteData.size() == 32U
                        ? 0U : static_cast<std::size_t>(y) * 32U;
                    color = f18Color(paletteData, paletteOffset, index);
                }
                if (!multicolorData.empty()) {
                    color = average(color, hardwarePalette[halfUnderlay(multicolorData, x, y)]);
                }
                const std::size_t output = static_cast<std::size_t>(x) * 3U;
                std::copy(color.begin(), color.end(), row.begin() + output);
            }
        }
    }
    return success(std::move(*image));
}

ImageDecodeResult decodeMsxScreen2(std::span<const std::uint8_t> bytes,
                                   const ImageSizeLimits& limits)
{
    constexpr std::size_t required = 0x2007U + graphicsTableSize;
    if (bytes.size() < required) {
        return failure(ImageDecodeError::TruncatedData, "MSX Screen 2 file is truncated.");
    }
    return decodeGraphics2(bytes.subspan(7, graphicsTableSize),
                           bytes.subspan(0x2007, graphicsTableSize), {}, {}, limits);
}

ImageDecodeResult decodeColecoCvPaint(std::span<const std::uint8_t> bytes,
                                      const ImageSizeLimits& limits)
{
    if (bytes.size() < graphicsTableSize * 2U) {
        return failure(ImageDecodeError::TruncatedData, "Coleco CVPaint file is truncated.");
    }
    return decodeGraphics2(bytes.first(graphicsTableSize),
                           bytes.subspan(graphicsTableSize, graphicsTableSize), {}, {}, limits);
}

ImageDecodeResult decodeAdamPowerPaint(std::span<const std::uint8_t> bytes,
                                       const ImageSizeLimits& limits)
{
    constexpr std::size_t activeSize = 0x1400U;
    if (bytes.size() < activeSize * 2U) {
        return failure(ImageDecodeError::TruncatedData, "Adam PowerPaint file is truncated.");
    }
    std::array<std::uint8_t, graphicsTableSize> patterns{};
    std::array<std::uint8_t, graphicsTableSize> colors{};
    colors.fill(0xf1U);
    std::copy_n(bytes.begin(), activeSize, patterns.begin());
    std::copy_n(bytes.begin() + activeSize, activeSize, colors.begin());
    return decodeGraphics2(patterns, colors, {}, {}, limits);
}

ImageDecodeResult decodeAdamHgr(std::span<const std::uint8_t> bytes,
                               const ImageSizeLimits& limits)
{
    constexpr std::size_t headerSize = 0x15U;
    constexpr std::size_t activeSize = 0x1400U;
    if (bytes.size() < headerSize + activeSize * 2U) {
        return failure(ImageDecodeError::TruncatedData, "Adam HGR file is truncated.");
    }
    std::array<std::uint8_t, graphicsTableSize> patterns{};
    std::array<std::uint8_t, graphicsTableSize> colors{};
    colors.fill(0xf1U);
    std::copy_n(bytes.begin() + headerSize, activeSize, colors.begin());
    std::copy_n(bytes.begin() + headerSize + activeSize, activeSize, patterns.begin());
    return decodeGraphics2(patterns, colors, {}, {}, limits);
}

} // namespace newconvert9918::core
