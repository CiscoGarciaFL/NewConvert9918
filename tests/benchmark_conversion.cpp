#include "newconvert9918/core/Bitmap9918Converter.hpp"
#include "newconvert9918/core/ConversionPerformance.hpp"
#include "newconvert9918/core/F18AConverter.hpp"
#include "newconvert9918/core/Multicolor9918Converter.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>

using namespace newconvert9918::core;

namespace {

template<typename Operation>
bool measure(std::string_view name,
             ConversionSettings settings,
             Operation operation)
{
    const auto start = std::chrono::steady_clock::now();
    const ConversionResult result = operation(settings);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const double milliseconds = std::chrono::duration<double, std::milli>(elapsed).count();
    const ConversionMemoryEstimate memory = estimateConversionMemory(settings);
    std::cout << std::left << std::setw(34) << name << std::right
              << std::setw(12) << std::fixed << std::setprecision(2) << milliseconds
              << std::setw(14) << memory.totalBytes() << '\n';
    return result.succeeded();
}

} // namespace

int main()
{
    auto source = RgbImage::createTightlyPacked(256, 192, PixelFormat::Rgb888);
    if (!source) {
        return 1;
    }
    for (std::uint32_t y = 0; y < source->height(); ++y) {
        std::span<std::uint8_t> row = source->row(y);
        for (std::uint32_t x = 0; x < source->width(); ++x) {
            const std::size_t offset = static_cast<std::size_t>(x) * 3;
            row[offset] = static_cast<std::uint8_t>((x * 5U + y * 3U) & 0xffU);
            row[offset + 1] = static_cast<std::uint8_t>((x * 2U + y * 7U) & 0xffU);
            row[offset + 2] = static_cast<std::uint8_t>((x * 11U + y) & 0xffU);
        }
    }
    const Palette palette = defaultBitmap9918Palette();
    ConversionSettings settings;

    std::cout << "mode                                      ms   buffer bytes\n";
    bool succeeded = true;
    settings.mode = ConversionMode::Bitmap9918;
    succeeded &= measure("Bitmap 9918A", settings, [&](const auto& value) {
        return convertBitmap9918(*source, palette, value);
    });
    settings.mode = ConversionMode::GreyscaleBitmap9918;
    succeeded &= measure("Greyscale Bitmap 9918A", settings, [&](const auto& value) {
        return convertGreyscaleBitmap9918(*source, palette, value);
    });
    settings.mode = ConversionMode::BlackAndWhiteBitmap9918;
    succeeded &= measure("Black-and-White Bitmap 9918A", settings, [&](const auto& value) {
        return convertBlackAndWhiteBitmap9918(*source, palette, value);
    });
    settings.mode = ConversionMode::BitmapColorOnly9918;
    succeeded &= measure("Bitmap Color Only 9918A", settings, [&](const auto& value) {
        return convertBitmapColorOnly9918(*source, palette, value);
    });
    settings.mode = ConversionMode::Multicolor9918;
    succeeded &= measure("Multicolor 9918", settings, [&](const auto& value) {
        return convertMulticolor9918(*source, palette, value);
    });
    settings.mode = ConversionMode::DualMulticolor9918;
    succeeded &= measure("Dual Multicolor 9918", settings, [&](const auto& value) {
        return convertDualMulticolor9918(*source, palette, value);
    });
    settings.mode = ConversionMode::HalfMulticolor9918;
    succeeded &= measure("Half Multicolor 9918A", settings, [&](const auto& value) {
        return convertHalfMulticolor9918(*source, palette, value);
    });
    settings.mode = ConversionMode::PalettedBitmapF18A;
    succeeded &= measure("Paletted Bitmap F18A", settings, [&](const auto& value) {
        return convertPalettedBitmapF18A(*source, palette, value);
    });
    settings.mode = ConversionMode::ScanlinePaletteBitmapF18A;
    succeeded &= measure("Scanline Palette Bitmap F18A", settings, [&](const auto& value) {
        return convertScanlinePaletteBitmapF18A(*source, value);
    });
    return succeeded ? 0 : 1;
}
