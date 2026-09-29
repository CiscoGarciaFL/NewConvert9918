#pragma once

#include "retrovdp/core/ConversionSettings.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace retrovdp::core {

enum class TargetProfileStatus : std::uint8_t {
    Implemented,
    Planned,
};

enum class TargetCapability : std::uint32_t {
    None = 0,
    FixedPalette = 1U << 0U,
    ProgrammablePalette = 1U << 1U,
    CharacterPatterns = 1U << 2U,
    Sprites = 1U << 3U,
    TileMaps = 1U << 4U,
    BitmapConversion = 1U << 5U,
    EnhancedColor = 1U << 6U,
    MultipleTileLayers = 1U << 7U,
};

[[nodiscard]] constexpr TargetCapability operator|(TargetCapability left,
                                                   TargetCapability right)
{
    return static_cast<TargetCapability>(
        static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

[[nodiscard]] constexpr bool hasCapability(TargetCapability value,
                                           TargetCapability requested)
{
    return (static_cast<std::uint32_t>(value)
            & static_cast<std::uint32_t>(requested))
        == static_cast<std::uint32_t>(requested);
}

struct TargetProfile {
    TargetProfileId id{TargetProfileId::Tms9918A};
    std::string_view stableId;
    std::string_view displayName;
    TargetProfileStatus status{TargetProfileStatus::Planned};
    std::uint32_t nominalVramBytes{};
    TargetCapability capabilities{TargetCapability::None};
    std::span<const ConversionMode> conversionModes;
};

[[nodiscard]] std::span<const TargetProfile> targetProfiles();
[[nodiscard]] const TargetProfile& targetProfile(TargetProfileId id);
[[nodiscard]] std::optional<TargetProfileId> targetProfileId(std::string_view stableId);
[[nodiscard]] bool supportsConversionMode(TargetProfileId profile,
                                          ConversionMode mode);
[[nodiscard]] TargetProfileId primaryTargetProfile(ConversionMode mode);
[[nodiscard]] TargetProfileId effectiveTargetProfile(TargetProfileId requested,
                                                      ConversionMode mode);
[[nodiscard]] ConversionMode defaultConversionMode(TargetProfileId profile);

} // namespace retrovdp::core
