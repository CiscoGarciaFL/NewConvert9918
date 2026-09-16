#pragma once

#include "newconvert9918/core/ConversionSettings.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace newconvert9918::core {

struct RgbColor {
    std::uint8_t red{};
    std::uint8_t green{};
    std::uint8_t blue{};

    [[nodiscard]] friend constexpr bool operator==(const RgbColor&, const RgbColor&) = default;
};

enum class PaletteError : std::uint8_t {
    None,
    Empty,
    TooManyColors,
};

class Palette final {
public:
    static constexpr std::size_t maximumColorCount = 16;

    [[nodiscard]] static std::optional<Palette>
    create(std::vector<RgbColor> colors, PaletteError* error = nullptr);

    [[nodiscard]] std::size_t size() const { return colors_.size(); }
    [[nodiscard]] bool empty() const { return colors_.empty(); }
    [[nodiscard]] std::span<const RgbColor> colors() const { return colors_; }
    [[nodiscard]] const RgbColor& at(std::size_t index) const;

private:
    explicit Palette(std::vector<RgbColor> colors);

    std::vector<RgbColor> colors_;
};

enum class TargetTableRole : std::uint8_t {
    Pattern,
    Color,
    Multicolor,
    MulticolorFrame1,
    MulticolorFrame2,
    FixedPattern,
    Palette,
    ScanlinePalettes,
};

struct TargetTableLayout {
    TargetTableRole role{TargetTableRole::Pattern};
    std::size_t byteSize{};
};

struct TargetMemoryTable {
    TargetTableRole role{TargetTableRole::Pattern};
    std::vector<std::uint8_t> bytes;
};

struct TargetMemoryImage {
    ConversionMode mode{ConversionMode::Bitmap9918};
    std::optional<Palette> palette;
    std::vector<TargetMemoryTable> tables;
};

enum class TargetTableError : std::uint8_t {
    None,
    UnsupportedConversionMode,
    TableCountMismatch,
    TableRoleMismatch,
    TableSizeMismatch,
};

struct TargetTableValidation {
    TargetTableError error{TargetTableError::None};
    std::size_t tableIndex{};
    TargetTableLayout expected{};

    [[nodiscard]] explicit constexpr operator bool() const
    {
        return error == TargetTableError::None;
    }
};

[[nodiscard]] std::span<const TargetTableLayout>
expectedTargetTables(ConversionMode mode);

[[nodiscard]] TargetTableValidation
validateTargetTables(ConversionMode mode, std::span<const TargetMemoryTable> tables);

[[nodiscard]] inline TargetTableValidation validateTargetTables(const TargetMemoryImage& image)
{
    return validateTargetTables(image.mode, image.tables);
}

} // namespace newconvert9918::core
