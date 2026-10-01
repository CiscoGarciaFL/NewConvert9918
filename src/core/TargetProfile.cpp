#include "retrovdp/core/TargetProfile.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace retrovdp::core {
namespace {

template <typename Id>
Id mustId(std::string_view value)
{
    auto id = Id::create(value);
    if (!id) throw std::logic_error("invalid stable registry identifier");
    return std::move(*id);
}

constexpr std::array tms9918Modes{
    ConversionMode::Bitmap9918, ConversionMode::GreyscaleBitmap9918,
    ConversionMode::BlackAndWhiteBitmap9918, ConversionMode::Multicolor9918,
    ConversionMode::DualMulticolor9918, ConversionMode::HalfMulticolor9918,
    ConversionMode::BitmapColorOnly9918,
};

constexpr std::array f18aModes{
    ConversionMode::Bitmap9918, ConversionMode::GreyscaleBitmap9918,
    ConversionMode::BlackAndWhiteBitmap9918, ConversionMode::Multicolor9918,
    ConversionMode::DualMulticolor9918, ConversionMode::HalfMulticolor9918,
    ConversionMode::BitmapColorOnly9918, ConversionMode::PalettedBitmapF18A,
    ConversionMode::ScanlinePaletteBitmapF18A,
};

constexpr std::array<ConversionMode, 0> v9938Modes{};

constexpr TargetCapability commonCapabilities =
    TargetCapability::FixedPalette | TargetCapability::CharacterPatterns
    | TargetCapability::Sprites | TargetCapability::TileMaps
    | TargetCapability::BitmapConversion;

const std::array modes{
    DisplayModeDescriptor{ConversionMode::Bitmap9918, mustId<ModeId>("bitmap-9918a"),
                          "Bitmap 9918A", TargetProfileId::Tms9918A,
                          {256, 192, {1, 1}}, {PaletteModel::Fixed, 16, 15, 0},
                          ModeOption::WorkingPalette},
    DisplayModeDescriptor{ConversionMode::GreyscaleBitmap9918,
                          mustId<ModeId>("greyscale-bitmap-9918a"),
                          "Greyscale Bitmap 9918A", TargetProfileId::Tms9918A,
                          {256, 192, {1, 1}}, {PaletteModel::Fixed, 16, 15, 0},
                          ModeOption::WorkingPalette},
    DisplayModeDescriptor{ConversionMode::BlackAndWhiteBitmap9918,
                          mustId<ModeId>("black-and-white-bitmap-9918a"),
                          "Black-and-White Bitmap 9918A", TargetProfileId::Tms9918A,
                          {256, 192, {1, 1}}, {PaletteModel::Fixed, 16, 15, 0},
                          ModeOption::WorkingPalette},
    DisplayModeDescriptor{ConversionMode::Multicolor9918,
                          mustId<ModeId>("multicolor-9918"), "Multicolor 9918",
                          TargetProfileId::Tms9918A, {256, 192, {1, 1}},
                          {PaletteModel::Fixed, 16, 15, 0},
                          ModeOption::WorkingPalette
                              | ModeOption::MulticolorFlickerLimit},
    DisplayModeDescriptor{ConversionMode::DualMulticolor9918,
                          mustId<ModeId>("dual-multicolor-9918"),
                          "Dual Multicolor 9918", TargetProfileId::Tms9918A,
                          {256, 192, {1, 1}}, {PaletteModel::Fixed, 16, 15, 0},
                          ModeOption::WorkingPalette
                              | ModeOption::MulticolorFlickerLimit},
    DisplayModeDescriptor{ConversionMode::HalfMulticolor9918,
                          mustId<ModeId>("half-multicolor-9918a"),
                          "Half Multicolor 9918A", TargetProfileId::Tms9918A,
                          {256, 192, {1, 1}}, {PaletteModel::Fixed, 16, 15, 0},
                          ModeOption::WorkingPalette
                              | ModeOption::MulticolorFlickerLimit},
    DisplayModeDescriptor{ConversionMode::BitmapColorOnly9918,
                          mustId<ModeId>("bitmap-color-only-9918a"),
                          "Bitmap Color Only 9918A", TargetProfileId::Tms9918A,
                          {256, 192, {1, 1}}, {PaletteModel::Fixed, 16, 15, 0},
                          ModeOption::WorkingPalette},
    DisplayModeDescriptor{ConversionMode::PalettedBitmapF18A,
                          mustId<ModeId>("paletted-bitmap-f18a"),
                          "Paletted Bitmap F18A", TargetProfileId::F18A,
                          {256, 192, {1, 1}}, {PaletteModel::ProgrammableRgb, 16, 15, 4},
                          ModeOption::PaletteSelection},
    DisplayModeDescriptor{ConversionMode::ScanlinePaletteBitmapF18A,
                          mustId<ModeId>("scanline-palette-bitmap-f18a"),
                          "Scanline Palette Bitmap F18A", TargetProfileId::F18A,
                          {256, 192, {1, 1}}, {PaletteModel::ProgrammableRgb, 16, 15, 4},
                          ModeOption::PaletteSelection | ModeOption::ScanlinePalette},
};

const std::array profiles{
    TargetProfile{TargetProfileId::Tms9918A, mustId<TargetId>("tms9918a"), "TMS9918A",
                  TargetKind::VideoDisplayProcessor, TargetProfileStatus::Implemented,
                  16U * 1024U, commonCapabilities, {8, 8, 256, 3, 32, 24},
                  {8, 16, 32, 32, 1, true, false},
                  tms9918Modes},
    TargetProfile{TargetProfileId::F18A, mustId<TargetId>("f18a"), "F18A",
                  TargetKind::VideoDisplayProcessor, TargetProfileStatus::Implemented,
                  18U * 1024U,
                  commonCapabilities | TargetCapability::ProgrammablePalette
                      | TargetCapability::EnhancedColor
                      | TargetCapability::MultipleTileLayers,
                  {8, 8, 256, 3, 32, 24},
                  {8, 16, 32, 32, 3, false, true},
                  f18aModes},
    TargetProfile{TargetProfileId::V9938, mustId<TargetId>("v9938"), "V9938",
                  TargetKind::VideoDisplayProcessor, TargetProfileStatus::Planned,
                  128U * 1024U,
                  TargetCapability::ProgrammablePalette
                      | TargetCapability::CharacterPatterns | TargetCapability::Sprites
                      | TargetCapability::TileMaps | TargetCapability::BitmapConversion
                  | TargetCapability::EnhancedColor,
                  {8, 8, 256, 3, 32, 24},
                  {8, 16, 32, 32, 1, true, false},
                  v9938Modes},
};

} // namespace

std::span<const TargetProfile> targetProfiles() { return profiles; }
std::span<const DisplayModeDescriptor> displayModes() { return modes; }

const TargetProfile& targetProfile(TargetProfileId id)
{
    const auto found = std::ranges::find(profiles, id, &TargetProfile::id);
    if (found == profiles.end()) throw std::out_of_range("unknown target profile");
    return *found;
}

std::optional<TargetProfileId> targetProfileId(std::string_view stableId)
{
    const auto found = std::ranges::find_if(profiles, [stableId](const auto& profile) {
        return profile.stableId == stableId;
    });
    if (found == profiles.end()) return std::nullopt;
    return found->id;
}

std::optional<TargetProfileId> targetProfileId(const TargetId& stableId)
{
    return targetProfileId(stableId.value());
}

const DisplayModeDescriptor& displayMode(ConversionMode mode)
{
    const auto found = std::ranges::find(modes, mode, &DisplayModeDescriptor::legacyMode);
    if (found == modes.end()) throw std::out_of_range("unknown display mode");
    return *found;
}

const DisplayModeDescriptor* findDisplayMode(const ModeId& stableId)
{
    const auto found = std::ranges::find_if(modes, [&stableId](const auto& mode) {
        return mode.stableId == stableId;
    });
    return found == modes.end() ? nullptr : &*found;
}

std::optional<ConversionMode> conversionMode(std::string_view stableId)
{
    const auto found = std::ranges::find_if(modes, [stableId](const auto& mode) {
        return mode.stableId == stableId;
    });
    if (found == modes.end()) return std::nullopt;
    return found->legacyMode;
}

std::optional<ConversionMode> conversionMode(const ModeId& stableId)
{
    const auto* descriptor = findDisplayMode(stableId);
    return descriptor == nullptr ? std::nullopt : std::optional(descriptor->legacyMode);
}

RegistryValidation validateRegistry(std::span<const TargetProfile> targetProfiles,
                                    std::span<const DisplayModeDescriptor> displayModes)
{
    for (std::size_t index = 0; index < displayModes.size(); ++index) {
        const auto& mode = displayModes[index];
        if (mode.geometry.width == 0 || mode.geometry.height == 0
            || mode.geometry.pixelAspectRatio.numerator == 0
            || mode.geometry.pixelAspectRatio.denominator == 0) {
            return {RegistryError::InvalidGeometry, index};
        }
        if (mode.palette.entryCount == 0 || mode.palette.workingColorCount == 0
            || mode.palette.workingColorCount > mode.palette.entryCount
            || (mode.palette.model == PaletteModel::ProgrammableRgb
                && mode.palette.channelBits == 0)) {
            return {RegistryError::InvalidPalette, index};
        }
        for (std::size_t earlier = 0; earlier < index; ++earlier) {
            if (displayModes[earlier].legacyMode == mode.legacyMode)
                return {RegistryError::DuplicateMode, index};
            if (displayModes[earlier].stableId == mode.stableId)
                return {RegistryError::DuplicateModeId, index};
        }
    }

    for (std::size_t index = 0; index < targetProfiles.size(); ++index) {
        const auto& profile = targetProfiles[index];
        if (profile.status == TargetProfileStatus::Implemented
            && profile.conversionModes.empty()) {
            return {RegistryError::ImplementedTargetHasNoModes, index};
        }
        if (hasCapability(profile.capabilities, TargetCapability::CharacterPatterns)) {
            const auto& patterns = profile.characterPatterns;
            if (patterns.pixelWidth == 0 || patterns.pixelHeight == 0
                || patterns.patternsPerSet == 0 || patterns.setCount == 0
                || patterns.mapColumns == 0 || patterns.mapRows == 0) {
                return {RegistryError::InvalidCharacterPatterns, index};
            }
        }
        if (hasCapability(profile.capabilities, TargetCapability::Sprites)) {
            const auto& sprites = profile.sprites;
            if (sprites.minimumPixelSize == 0
                || sprites.maximumPixelSize < sprites.minimumPixelSize
                || sprites.patternsPerSet == 0 || sprites.maximumVisibleSprites == 0
                || sprites.maximumColorDepth == 0
                || sprites.maximumColorDepth > 8
                || (sprites.usesGlobalSize && sprites.supportsPerSpriteSize)) {
                return {RegistryError::InvalidSprites, index};
            }
        }
        for (std::size_t earlier = 0; earlier < index; ++earlier) {
            if (targetProfiles[earlier].id == profile.id)
                return {RegistryError::DuplicateTargetProfile, index};
            if (targetProfiles[earlier].stableId == profile.stableId)
                return {RegistryError::DuplicateTargetId, index};
        }
        for (std::size_t modeIndex = 0; modeIndex < profile.conversionModes.size(); ++modeIndex) {
            const ConversionMode mode = profile.conversionModes[modeIndex];
            if (std::ranges::find(displayModes, mode,
                                  &DisplayModeDescriptor::legacyMode) == displayModes.end()) {
                return {RegistryError::UnknownTargetMode, index};
            }
            const auto preceding = profile.conversionModes.first(modeIndex);
            if (std::ranges::find(preceding, mode) != preceding.end())
                return {RegistryError::DuplicateTargetMode, index};
        }
    }

    for (std::size_t index = 0; index < displayModes.size(); ++index) {
        const auto& mode = displayModes[index];
        const auto target = std::ranges::find(targetProfiles, mode.primaryTarget,
                                              &TargetProfile::id);
        if (target == targetProfiles.end())
            return {RegistryError::UnknownPrimaryTarget, index};
        if (std::ranges::find(target->conversionModes, mode.legacyMode)
            == target->conversionModes.end()) {
            return {RegistryError::PrimaryTargetDoesNotSupportMode, index};
        }
    }
    return {};
}

RegistryValidation validateRegistry() { return validateRegistry(profiles, modes); }

bool supportsConversionMode(TargetProfileId profile, ConversionMode mode)
{
    const auto supported = targetProfile(profile).conversionModes;
    return std::ranges::find(supported, mode) != supported.end();
}

TargetProfileId primaryTargetProfile(ConversionMode mode)
{
    return displayMode(mode).primaryTarget;
}

TargetProfileId effectiveTargetProfile(TargetProfileId requested, ConversionMode mode)
{
    return supportsConversionMode(requested, mode) ? requested : primaryTargetProfile(mode);
}

ConversionMode defaultConversionMode(TargetProfileId profile)
{
    const auto supported = targetProfile(profile).conversionModes;
    if (supported.empty()) return ConversionMode::Bitmap9918;
    if (profile == TargetProfileId::F18A) return ConversionMode::PalettedBitmapF18A;
    return supported.front();
}

} // namespace retrovdp::core
