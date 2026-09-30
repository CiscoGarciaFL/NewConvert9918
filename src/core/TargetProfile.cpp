#include "retrovdp/core/TargetProfile.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace retrovdp::core {
namespace {

constexpr std::array tms9918Modes{
    ConversionMode::Bitmap9918,
    ConversionMode::GreyscaleBitmap9918,
    ConversionMode::BlackAndWhiteBitmap9918,
    ConversionMode::Multicolor9918,
    ConversionMode::DualMulticolor9918,
    ConversionMode::HalfMulticolor9918,
    ConversionMode::BitmapColorOnly9918,
};

constexpr std::array f18aModes{
    ConversionMode::Bitmap9918,
    ConversionMode::GreyscaleBitmap9918,
    ConversionMode::BlackAndWhiteBitmap9918,
    ConversionMode::Multicolor9918,
    ConversionMode::DualMulticolor9918,
    ConversionMode::HalfMulticolor9918,
    ConversionMode::BitmapColorOnly9918,
    ConversionMode::PalettedBitmapF18A,
    ConversionMode::ScanlinePaletteBitmapF18A,
};

constexpr std::array<ConversionMode, 0> v9938Modes{};

constexpr TargetCapability commonCapabilities =
    TargetCapability::FixedPalette
    | TargetCapability::CharacterPatterns
    | TargetCapability::Sprites
    | TargetCapability::TileMaps
    | TargetCapability::BitmapConversion;

constexpr std::array profiles{
    TargetProfile{
        TargetProfileId::Tms9918A,
        "tms9918a",
        "TMS9918A",
        TargetProfileStatus::Implemented,
        16U * 1024U,
        commonCapabilities,
        tms9918Modes,
    },
    TargetProfile{
        TargetProfileId::F18A,
        "f18a",
        "F18A",
        TargetProfileStatus::Implemented,
        18U * 1024U,
        commonCapabilities
            | TargetCapability::ProgrammablePalette
            | TargetCapability::EnhancedColor
            | TargetCapability::MultipleTileLayers,
        f18aModes,
    },
    TargetProfile{
        TargetProfileId::V9938,
        "v9938",
        "V9938",
        TargetProfileStatus::Planned,
        128U * 1024U,
        TargetCapability::ProgrammablePalette
            | TargetCapability::CharacterPatterns
            | TargetCapability::Sprites
            | TargetCapability::TileMaps
            | TargetCapability::BitmapConversion
            | TargetCapability::EnhancedColor,
        v9938Modes,
    },
};

} // namespace

std::span<const TargetProfile> targetProfiles()
{
    return profiles;
}

const TargetProfile& targetProfile(TargetProfileId id)
{
    const auto found = std::ranges::find(profiles, id, &TargetProfile::id);
    if (found == profiles.end()) throw std::out_of_range("unknown target profile");
    return *found;
}

std::optional<TargetProfileId> targetProfileId(std::string_view stableId)
{
    const auto found = std::ranges::find(profiles, stableId, &TargetProfile::stableId);
    if (found == profiles.end()) return std::nullopt;
    return found->id;
}

bool supportsConversionMode(TargetProfileId profile, ConversionMode mode)
{
    const auto modes = targetProfile(profile).conversionModes;
    return std::ranges::find(modes, mode) != modes.end();
}

TargetProfileId primaryTargetProfile(ConversionMode mode)
{
    switch (mode) {
    case ConversionMode::PalettedBitmapF18A:
    case ConversionMode::ScanlinePaletteBitmapF18A:
        return TargetProfileId::F18A;
    default:
        return TargetProfileId::Tms9918A;
    }
}

TargetProfileId effectiveTargetProfile(TargetProfileId requested,
                                       ConversionMode mode)
{
    return supportsConversionMode(requested, mode)
        ? requested : primaryTargetProfile(mode);
}

ConversionMode defaultConversionMode(TargetProfileId profile)
{
    const auto modes = targetProfile(profile).conversionModes;
    if (modes.empty()) return ConversionMode::Bitmap9918;
    if (profile == TargetProfileId::F18A) return ConversionMode::PalettedBitmapF18A;
    return modes.front();
}

} // namespace retrovdp::core
