#include "newconvert9918/core/TargetData.hpp"

#include <array>
#include <stdexcept>
#include <utility>

namespace newconvert9918::core {
namespace {

constexpr std::array bitmap9918Tables{
    TargetTableLayout{TargetTableRole::Pattern, 6144},
    TargetTableLayout{TargetTableRole::Color, 6144},
};
constexpr std::array blackAndWhite9918Tables{
    TargetTableLayout{TargetTableRole::Pattern, 6144},
};
constexpr std::array multicolor9918Tables{
    TargetTableLayout{TargetTableRole::Multicolor, 1536},
};
constexpr std::array dualMulticolor9918Tables{
    TargetTableLayout{TargetTableRole::MulticolorFrame1, 1536},
    TargetTableLayout{TargetTableRole::MulticolorFrame2, 1536},
};
constexpr std::array halfMulticolor9918Tables{
    TargetTableLayout{TargetTableRole::Pattern, 6144},
    TargetTableLayout{TargetTableRole::Color, 6144},
    TargetTableLayout{TargetTableRole::Multicolor, 2048},
};
constexpr std::array bitmapColorOnly9918Tables{
    TargetTableLayout{TargetTableRole::FixedPattern, 6144},
    TargetTableLayout{TargetTableRole::Color, 6144},
};
constexpr std::array palettedBitmapF18ATables{
    TargetTableLayout{TargetTableRole::Pattern, 6144},
    TargetTableLayout{TargetTableRole::Color, 6144},
    TargetTableLayout{TargetTableRole::Palette, 32},
};
constexpr std::array scanlinePaletteBitmapF18ATables{
    TargetTableLayout{TargetTableRole::Pattern, 6144},
    TargetTableLayout{TargetTableRole::Color, 6144},
    TargetTableLayout{TargetTableRole::ScanlinePalettes, 6144},
};

void setPaletteError(PaletteError* destination, PaletteError error)
{
    if (destination != nullptr) {
        *destination = error;
    }
}

} // namespace

std::optional<Palette> Palette::create(std::vector<RgbColor> colors, PaletteError* error)
{
    if (colors.empty()) {
        setPaletteError(error, PaletteError::Empty);
        return std::nullopt;
    }
    if (colors.size() > maximumColorCount) {
        setPaletteError(error, PaletteError::TooManyColors);
        return std::nullopt;
    }

    setPaletteError(error, PaletteError::None);
    return Palette(std::move(colors));
}

const RgbColor& Palette::at(std::size_t index) const
{
    return colors_.at(index);
}

Palette::Palette(std::vector<RgbColor> colors)
    : colors_(std::move(colors))
{
}

std::span<const TargetTableLayout> expectedTargetTables(ConversionMode mode)
{
    switch (mode) {
    case ConversionMode::Bitmap9918:
    case ConversionMode::GreyscaleBitmap9918:
        return bitmap9918Tables;
    case ConversionMode::BlackAndWhiteBitmap9918:
        return blackAndWhite9918Tables;
    case ConversionMode::Multicolor9918:
        return multicolor9918Tables;
    case ConversionMode::DualMulticolor9918:
        return dualMulticolor9918Tables;
    case ConversionMode::HalfMulticolor9918:
        return halfMulticolor9918Tables;
    case ConversionMode::BitmapColorOnly9918:
        return bitmapColorOnly9918Tables;
    case ConversionMode::PalettedBitmapF18A:
        return palettedBitmapF18ATables;
    case ConversionMode::ScanlinePaletteBitmapF18A:
        return scanlinePaletteBitmapF18ATables;
    }
    return {};
}

TargetTableValidation validateTargetTables(ConversionMode mode,
                                           std::span<const TargetMemoryTable> tables)
{
    const std::span<const TargetTableLayout> expected = expectedTargetTables(mode);
    if (expected.empty()) {
        return {.error = TargetTableError::UnsupportedConversionMode};
    }
    if (tables.size() != expected.size()) {
        const std::size_t mismatchIndex =
            tables.size() < expected.size() ? tables.size() : expected.size();
        return {
            .error = TargetTableError::TableCountMismatch,
            .tableIndex = mismatchIndex,
            .expected = mismatchIndex < expected.size() ? expected[mismatchIndex]
                                                        : TargetTableLayout{},
        };
    }

    for (std::size_t index = 0; index < expected.size(); ++index) {
        if (tables[index].role != expected[index].role) {
            return {
                .error = TargetTableError::TableRoleMismatch,
                .tableIndex = index,
                .expected = expected[index],
            };
        }
        if (tables[index].bytes.size() != expected[index].byteSize) {
            return {
                .error = TargetTableError::TableSizeMismatch,
                .tableIndex = index,
                .expected = expected[index],
            };
        }
    }
    return {};
}

} // namespace newconvert9918::core
